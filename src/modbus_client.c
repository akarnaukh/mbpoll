#include "modbus_client.h"
#include "websocket.h"
#include "daemon.h"
#include <errno.h>
#include <string.h>
#include <unistd.h>

// Функция выполнения команды записи Modbus (публичная)
int execute_modbus_write(config_t *config, modbus_write_command_t *cmd) {
    if (!config->mb_ctx) {
        log_error("Modbus context is NULL");
        return -1;
    }
    
    // Устанавливаем адрес устройства
    modbus_set_slave(config->mb_ctx, cmd->unit_id);
    
    int rc = -1;
    switch (cmd->function_code) {
        case 5: // Write Single Coil
            log_debug("Writing single coil: addr=%d, value=%d", cmd->address, cmd->value ? 0xFF00 : 0);
            rc = modbus_write_bit(config->mb_ctx, cmd->address, cmd->value ? 0xFF00 : 0);
            break;
            
        case 6: // Write Single Register
            log_debug("Writing single register: addr=%d, value=%d", cmd->address, cmd->value);
            rc = modbus_write_register(config->mb_ctx, cmd->address, (uint16_t)cmd->value);
            break;
            
        case 15: // Write Multiple Coils
            log_debug("Writing multiple coils: addr=%d, quantity=%d", cmd->address, cmd->quantity);
            if (cmd->values) {
                uint8_t *coils = malloc(cmd->quantity * sizeof(uint8_t));
                if (coils) {
                    for (int i = 0; i < cmd->quantity; i++) {
                        coils[i] = cmd->values[i] ? 1 : 0;
                    }
                    rc = modbus_write_bits(config->mb_ctx, cmd->address, cmd->quantity, coils);
                    free(coils);
                }
            }
            break;
            
        case 16: // Write Multiple Registers
            log_debug("Writing multiple registers: addr=%d, quantity=%d", cmd->address, cmd->quantity);
            if (cmd->values) {
                rc = modbus_write_registers(config->mb_ctx, cmd->address, cmd->quantity, cmd->values);
            }
            break;
            
        default:
            log_error("Unsupported function code: %d", cmd->function_code);
            return -1;
    }
    
    if (rc == -1) {
        log_error("Write command failed: %s", modbus_strerror(errno));
        return -1;
    }
    
    log_info("Write command successful: fc=%d, unit=%d, addr=%d, qty=%d", 
             cmd->function_code, cmd->unit_id, cmd->address, cmd->quantity);
    return 0;
}

// Функция выполнения команды записи Modbus (приватная, для обратной совместимости)
static int execute_write_command(config_t *config, modbus_write_command_t *cmd) {
    return execute_modbus_write(config, cmd);
}

// Проверка и выполнение команд записи из очереди
// Возвращает 1 если команда была выполнена, 0 если очередь пуста
// Выполняет запись немедленно, не дожидаясь окончания интервала опроса
int check_and_execute_write_commands(config_t *config) {
    if (!config || config->write_command_count == 0) {
        return 0;
    }
    
    pthread_mutex_lock(&config->write_queue_mutex);
    
    if (config->write_command_count == 0) {
        pthread_mutex_unlock(&config->write_queue_mutex);
        return 0;
    }
    
    // Берем первую команду из очереди
    modbus_write_command_t cmd = config->write_commands[0];
    
    // Сдвигаем очередь
    if (config->write_command_count > 1) {
        memmove(&config->write_commands[0], &config->write_commands[1],
                (config->write_command_count - 1) * sizeof(modbus_write_command_t));
    }
    config->write_command_count--;
    
    pthread_mutex_unlock(&config->write_queue_mutex);
    
    // Устанавливаем флаг приостановки опроса
    config->pause_polling = 1;
    
    // Выполняем команду записи
    int success = (execute_write_command(config, &cmd) == 0);
    
    // Отправляем результат в WebSocket с деталями запроса
    if (success) {
        send_write_result_to_websocket(config, 1, NULL, &cmd);
    } else {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "%s", modbus_strerror(errno));
        send_write_result_to_websocket(config, 0, error_msg, &cmd);
    }
    
    // Освобождаем память команды
    free(cmd.values);
    
    // Сбрасываем флаг приостановки и продолжаем опрос
    config->pause_polling = 0;
    
    return 1;
}

// Немедленное выполнение команды записи Modbus (без ожидания интервала опроса)
// Если идет опрос чтения регистров устройства - ждем окончания чтения текущего устройства
// приостанавливаем опрос - выполняем запрос на запись - отправляем результат в WS
// параллельно запуская опрос дальше.
// Если идет ожидание следующего цикла опроса устройств - сразу отправляем запрос на запись,
// отправляем результат в WS, продолжаем ждать сколько осталось.
void execute_modbus_write_immediate(config_t *config, modbus_write_command_t *cmd) {
    if (!config || !cmd) {
        return;
    }
    
    log_info("Executing immediate write command: fc=%d, unit=%d, addr=%d, qty=%d", 
             cmd->function_code, cmd->unit_id, cmd->address, cmd->quantity);
    
    // Устанавливаем флаг приостановки опроса
    config->pause_polling = 1;
    
    // Выполняем команду записи
    int success = (execute_modbus_write(config, cmd) == 0);
    
    // Отправляем результат в WebSocket с деталями запроса
    if (success) {
        send_write_result_to_websocket(config, 1, NULL, cmd);
    } else {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "%s", modbus_strerror(errno));
        send_write_result_to_websocket(config, 0, error_msg, cmd);
    }
    
    // Освобождаем память для массива значений если есть
    free(cmd->values);
    
    // Сбрасываем флаг приостановки и продолжаем опрос
    config->pause_polling = 0;
}

int init_modbus_connection(config_t *config) {
    log_info("Initializing Modbus connection...");
    
    if (config->is_serial) {
        log_debug("Creating serial Modbus context: %s@%d", 
                 config->serial_port, config->baud_rate);
        
        config->mb_ctx = modbus_new_rtu(config->serial_port, 
                                        config->baud_rate, 
                                        'N', 8, 1);
        if (!config->mb_ctx) {
            log_error("Failed to create serial Modbus context: %s", modbus_strerror(errno));
            return -1;
        }
        
        // Увеличиваем таймауты
        modbus_set_response_timeout(config->mb_ctx, 2, 0); // 2 секунды
        modbus_set_byte_timeout(config->mb_ctx, 1, 0);     // 1 секунда между байтами
        
        log_debug("Connecting to serial port...");
        if (modbus_connect(config->mb_ctx) == -1) {
            log_error("Serial connection failed: %s", modbus_strerror(errno));
            modbus_free(config->mb_ctx);
            config->mb_ctx = NULL;
            return -1;
        }
        
        log_info("Serial Modbus connection established: %s@%d", 
                config->serial_port, config->baud_rate);
    } else {
        log_debug("Creating TCP Modbus context: %s:%d", 
                 config->tcp_ip, config->tcp_port);
        
        config->mb_ctx = modbus_new_tcp(config->tcp_ip, config->tcp_port);
        if (!config->mb_ctx) {
            log_error("Failed to create TCP Modbus context: %s", modbus_strerror(errno));
            return -1;
        }
        
        // Увеличиваем таймауты для TCP
        modbus_set_response_timeout(config->mb_ctx, 3, 0); // 3 секунды
        modbus_set_byte_timeout(config->mb_ctx, 1, 500000); // 1.5 секунды между байтами
        
        // Проверяем доступность функции modbus_set_tcp_keepalive
        // Она может отсутствовать в некоторых версиях библиотеки
        #ifdef MODBUS_SET_TCP_KEEPALIVE
        modbus_set_tcp_keepalive(config->mb_ctx, 1); // Включить keepalive
        #else
        log_debug("modbus_set_tcp_keepalive not available in this libmodbus version");
        #endif
        
        log_debug("Connecting to TCP server...");
        if (modbus_connect(config->mb_ctx) == -1) {
            log_error("TCP connection failed: %s", modbus_strerror(errno));
            modbus_free(config->mb_ctx);
            config->mb_ctx = NULL;
            return -1;
        }
        
        log_info("TCP Modbus connection established: %s:%d", 
                config->tcp_ip, config->tcp_port);
    }
    
    log_debug("Modbus context created successfully");
    return 0;
}

int reinit_modbus_connection(config_t *config) {
    static int retry_count = 0;
    static int retry_delay = 2;
    
    log_info("Reinitializing Modbus connection (attempt %d)...", retry_count + 1);
    
    if (config->mb_ctx) {
        log_debug("Closing existing Modbus connection");
        modbus_close(config->mb_ctx);
        modbus_free(config->mb_ctx);
        config->mb_ctx = NULL;
    }
    
    // Экспоненциальная задержка с ограничением
    if (retry_count > 0) {
        log_debug("Waiting %d seconds before retry...", retry_delay);
        sleep(retry_delay);
        
        // Увеличиваем задержку для следующей попытки (макс 30 секунд)
        retry_delay = retry_delay * 2;
        if (retry_delay > 30) {
            retry_delay = 30;
        }
    }
    
    retry_count++;
    
    int result = init_modbus_connection(config);
    
    if (result == 0) {
        // Сбрасываем счетчик при успешном подключении
        retry_count = 0;
        retry_delay = 2;
    }
    
    return result;
}

int check_modbus_connection(config_t *config) {
    log_debug("Checking Modbus connection...");
    
    if (!config->mb_ctx) {
        log_debug("Modbus context is NULL");
        return 0;
    }
    
    // Сохраняем текущий slave адрес
    int current_slave = modbus_get_slave(config->mb_ctx);
    
    // Проверяем соединение, пытаясь прочитать фиктивный регистр
    // Используем slave адрес 1 для проверки
    modbus_set_slave(config->mb_ctx, 1);
    uint16_t dummy[1];
    
    // Сохраняем текущие таймауты
    uint32_t orig_to_sec, orig_to_usec;
    modbus_get_response_timeout(config->mb_ctx, &orig_to_sec, &orig_to_usec);
    
    // Устанавливаем короткий, но не слишком короткий таймаут для проверки
    modbus_set_response_timeout(config->mb_ctx, 1, 0); // 1 секунда
    
    int rc = modbus_read_registers(config->mb_ctx, 0, 1, dummy);
    
    // Восстанавливаем таймауты
    modbus_set_response_timeout(config->mb_ctx, orig_to_sec, orig_to_usec);
    
    // Восстанавливаем slave адрес
    modbus_set_slave(config->mb_ctx, current_slave);
    
    if (rc == -1) {
        int saved_errno = errno;
        const char *error_str = modbus_strerror(saved_errno);
        
        // Разные типы ошибок
        if (saved_errno == ETIMEDOUT) {
            log_debug("Modbus connection check: timeout (device may be busy)");
            // Таймаут не всегда означает разрыв соединения
            // Возможно устройство просто медленно отвечает
            return 1; // Считаем соединение живым
        } else if (saved_errno == ECONNREFUSED || saved_errno == ENETUNREACH) {
            log_debug("Modbus connection check failed: %s", error_str);
            return 0; // Соединение разорвано
        } else {
            log_debug("Modbus connection check error: %s", error_str);
            // Другие ошибки - считаем соединение живым
            return 1;
        }
    }
    
    log_debug("Modbus connection is OK");
    return 1;
}

void poll_all_devices(config_t *config) {
    log_debug("Starting poll of all devices");
    
    for (int i = 0; i < config->device_count; i++) {
        modbus_device_t *device = &config->devices[i];
        int device_timeout = 0;
        
        log_debug("Polling device %d", device->slave_address);
        
        // Сбрасываем флаг доступности перед опросом
        device->device_available = 1;
        
        for (int j = 0; j < device->range_count; j++) {
            register_range_t *range = &device->ranges[j];
            
            // Проверяем, не поступила ли команда записи - если да, приостанавливаем опрос
            if (config->pause_polling) {
                log_debug("Polling paused for write command, continuing to next cycle");
                return;
            }
            
            // Если устройство уже недоступно, пропускаем остальные диапазоны
            if (device_timeout) {
                log_debug("  Device %d timeout, skipping function %d", 
                         device->slave_address, range->function);
                if (range->values) {
                    free(range->values);
                    range->values = NULL;
                }
                continue;
            }
            
            // Устанавливаем адрес устройства
            modbus_set_slave(config->mb_ctx, device->slave_address);
            
            // Выделяем память для значений, если еще не выделена
            if (!range->values) {
                range->values = malloc(range->quantity * sizeof(uint16_t));
                if (!range->values) {
                    log_error("Memory allocation failed for device %d values", 
                             device->slave_address);
                    device_timeout = 1;
                    continue;
                }
            }
            
            // Читаем регистры в зависимости от функции
            int rc = -1;
            switch (range->function) {
                case 2: // Input bits
                    log_debug("  Reading input bits: start=%d, quantity=%d", 
                             range->start, range->quantity);
                    {
                        uint8_t *bits = malloc(range->quantity * sizeof(uint8_t));
                        if (bits) {
                            rc = modbus_read_input_bits(config->mb_ctx, 
                                                       range->start, 
                                                       range->quantity, 
                                                       bits);
                            if (rc == range->quantity) {
                                // Конвертируем биты в 16-битные значения
                                for (int k = 0; k < range->quantity; k++) {
                                    range->values[k] = bits[k];
                                }
                                log_debug("    Read %d input bits successfully", rc);
                            }
                            free(bits);
                        }
                    }
                    break;
                    
                case 3: // Holding registers
                    log_debug("  Reading holding registers: start=%d, quantity=%d", 
                             range->start, range->quantity);
                    rc = modbus_read_registers(config->mb_ctx, 
                                              range->start, 
                                              range->quantity, 
                                              range->values);
                    break;
                    
                case 4: // Input registers
                    log_debug("  Reading input registers: start=%d, quantity=%d", 
                             range->start, range->quantity);
                    rc = modbus_read_input_registers(config->mb_ctx, 
                                                    range->start, 
                                                    range->quantity, 
                                                    range->values);
                    break;
                    
                default:
                    log_warn("Unsupported Modbus function %d for device %d", 
                            range->function, device->slave_address);
                    continue;
            }
            
            if (rc == -1) {
                if (errno == ETIMEDOUT) {
                    log_warn("Timeout reading device %d (function %d)", 
                            device->slave_address, range->function);
                    device_timeout = 1;
                    device->device_available = 0;
                    
                    // Освобождаем память для значений
                    if (range->values) {
                        free(range->values);
                        range->values = NULL;
                    }
                    
                    // Пропускаем остальные диапазоны для этого устройства
                    continue;
                } else {
                    log_error("Error reading device %d: %s", 
                             device->slave_address, modbus_strerror(errno));
                    
                    if (range->values) {
                        free(range->values);
                        range->values = NULL;
                    }
                }
            } else if (rc != range->quantity) {
                log_warn("Partial read for device %d: %d/%d registers", 
                        device->slave_address, rc, range->quantity);
                // Частичное чтение - отмечаем устройство как недоступное
                device->device_available = 0;
                
                if (range->values) {
                    free(range->values);
                    range->values = NULL;
                }
            } else {
                if (config->log_level >= LOG_LEVEL_DEBUG) {
                    char debug_msg[1024];
                    int pos = snprintf(debug_msg, sizeof(debug_msg), 
                            "Device %d (func %d): ", 
                            device->slave_address, range->function);
                    
                    for (int k = 0; k < rc && k < 3 && pos < (int)sizeof(debug_msg) - 10; k++) {
                        pos += snprintf(debug_msg + pos, sizeof(debug_msg) - pos, 
                                       "%04X ", range->values[k]);
                    }
                    if (rc > 3) {
                        snprintf(debug_msg + pos, sizeof(debug_msg) - pos, "...");
                    }
                    
                    log_debug("%s", debug_msg);
                }
            }
            
            // Короткая пауза между запросами к одному устройству
            // Используем nanosleep вместо usleep для лучшей переносимости
            struct timespec short_delay;
            short_delay.tv_sec = 0;
            short_delay.tv_nsec = 50000000; // 50ms в наносекундах
            nanosleep(&short_delay, NULL);
        }
        
        // После опроса всех диапазонов устройства отправляем данные в WebSocket если включено
        if (config->ws_request_output && config->websocket_port > 0) {
            send_device_to_websockets(config, device);
        }
        
        // Пауза между устройствами
        struct timespec device_delay;
        device_delay.tv_sec = 0;
        device_delay.tv_nsec = 50000000; // 50ms в наносекундах
        nanosleep(&device_delay, NULL);
        
        if (device_timeout) {
            log_info("Device %d is unavailable (timeout)", device->slave_address);
        } else if (device->device_available) {
            log_debug("Device %d polled successfully", device->slave_address);
        }
    }
    
    log_debug("Completed poll of all devices");
}

