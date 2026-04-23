#include "daemon.h"
#include "config.h"
#include "modbus_client.h"
#include "device_list.h"
#include "websocket.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <config_file>\n", argv[0]);
        return EXIT_FAILURE;
    }
    
    // Открываем syslog
    openlog("mbusread", LOG_PID | LOG_CONS, LOG_DAEMON);
    log_info("=== Starting mbusread service ===");
    log_info("Configuration file: %s", argv[1]);
    
    // Инициализируем конфигурацию
    g_config = malloc(sizeof(config_t));
    if (!g_config) {
        log_error("Failed to allocate memory for configuration");
        return EXIT_FAILURE;
    }
    memset(g_config, 0, sizeof(config_t));
    
    // Устанавливаем значения по умолчанию
    g_config->poll_interval_ms = 1000;
    g_config->listing_port = 24122;
    g_config->websocket_port = 0; // Выключен по умолчанию
    g_config->ws_request_output = 0; // Выключен по умолчанию
    g_config->log_level = LOG_LEVEL_ERROR;
    
    // === ШАГ 1: Загрузка конфигурации ===
    log_info("=== Step 1: Loading configuration ===");
    if (load_config(argv[1], g_config) != 0) {
        log_error("Failed to load configuration");
        cleanup();
        return EXIT_FAILURE;
    }
    
    // === ШАГ 2: Загрузка списка устройств ===
    log_info("=== Step 2: Loading device list ===");
    if (load_device_list(g_config) != 0) {
        log_error("Failed to load device list");
        cleanup();
        return EXIT_FAILURE;
    }
    
    // === ШАГ 3: Инициализация Modbus соединения ===
    log_info("=== Step 3: Initializing Modbus connection ===");
    if (init_modbus_connection(g_config) != 0) {
        log_error("Failed to initialize Modbus connection");
        cleanup();
        return EXIT_FAILURE;
    }
    
    // === ШАГ 4: Проверка Modbus соединения ===
    log_info("=== Step 4: Checking Modbus connection ===");
    if (!check_modbus_connection(g_config)) {
        log_warn("Initial Modbus connection check failed");
        // Продолжаем, так как устройство может стать доступным позже
    } else {
        log_info("Modbus connection is OK");
    }
    
    // === ШАГ 5: Запуск WebSocket сервера (если включен) ===
    pthread_t websocket_thread = 0;
    if (g_config->websocket_port > 0) {
        log_info("=== Step 5: Starting WebSocket server ===");
        if (init_websocket_server(g_config) == 0) {
            if (pthread_create(&websocket_thread, NULL, websocket_server_thread, g_config) != 0) {
                log_error("Failed to create WebSocket server thread");
                // Продолжаем без WebSocket
            } else {
                pthread_detach(websocket_thread);
                log_info("WebSocket server started on port %d", g_config->websocket_port);
            }
        }
    } else {
        log_info("=== Step 5: WebSocket server disabled ===");
    }
    
    log_info("=== Service initialization complete ===");
    log_info("Poll interval: %d ms", g_config->poll_interval_ms);
    
    if (g_config->websocket_port > 0) {
        log_info("WebSocket server listening on %s:%d", 
                 g_config->listing_ip, g_config->websocket_port);
    }
    
    log_info("Ready to poll %d devices", g_config->device_count);
    
    // Устанавливаем обработчики сигналов
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);
    
    // === Главный цикл опроса ===
    log_info("=== Entering main polling loop ===");
    int loop_counter = 0;
    int successful_polls = 0;
    int failed_polls = 0;
    int connection_check_counter = 0;
    
    // Буфер для JSON данных
    char *last_json_data = NULL;
    size_t last_json_len = 0;
    
    while (g_running) {
        loop_counter++;
        connection_check_counter++;
        
        if (loop_counter % 10 == 0) {
            log_debug("=== Polling cycle %d ===", loop_counter);
        }
        
        pthread_mutex_lock(&g_data_mutex);
        
        // Проверяем доступность соединения не каждый раз
        int need_check = (connection_check_counter >= 5) || (failed_polls > 0);
        
        if (need_check && !check_modbus_connection(g_config)) {
            log_warn("Modbus connection lost, attempting to reconnect...");
            if (reinit_modbus_connection(g_config) != 0) {
                log_error("Failed to reinitialize Modbus connection");
                pthread_mutex_unlock(&g_data_mutex);
                sleep(5);
                failed_polls++;
                continue;
            }
            log_info("Modbus connection reestablished");
            connection_check_counter = 0;
        }
        
        // Опрашиваем все устройства
        poll_all_devices(g_config);
        successful_polls++;
        
        // Формируем JSON данные для WebSocket
        if (g_config->websocket_port > 0) {
            json_t *root = json_object();
            json_t *devices_array = json_array();
            
            for (int i = 0; i < g_config->device_count; i++) {
                modbus_device_t *device = &g_config->devices[i];
                
                json_t *device_obj = json_object();
                json_object_set_new(device_obj, "address", json_integer(device->slave_address));
                json_object_set_new(device_obj, "available", json_boolean(device->device_available));
                
                json_t *registers_array = json_array();
                
                for (int j = 0; j < device->range_count; j++) {
                    register_range_t *range = &device->ranges[j];
                    
                    json_t *range_obj = json_object();
                    json_object_set_new(range_obj, "function", json_integer(range->function));
                    json_object_set_new(range_obj, "start", json_integer(range->start));
                    json_object_set_new(range_obj, "quantity", json_integer(range->quantity));
                    
                    json_t *values_array = json_array();
                    if (range->values && device->device_available) {
                        for (int k = 0; k < range->quantity; k++) {
                            json_array_append_new(values_array, json_integer(range->values[k]));
                        }
                    } else {
                        for (int k = 0; k < range->quantity; k++) {
                            json_array_append_new(values_array, json_string("na"));
                        }
                    }
                    
                    json_object_set_new(range_obj, "values", values_array);
                    json_array_append_new(registers_array, range_obj);
                }
                
                json_object_set_new(device_obj, "registers", registers_array);
                json_array_append_new(devices_array, device_obj);
            }
            
            json_object_set_new(root, "devices", devices_array);
            json_object_set_new(root, "timestamp", json_integer(time(NULL)));
            json_object_set_new(root, "poll_cycle", json_integer(loop_counter));
            
            char *json_str = json_dumps(root, JSON_COMPACT);
            json_decref(root);
            
            if (json_str) {
                // Отправляем данные всем WebSocket клиентам
                broadcast_to_websockets(g_config, json_str, strlen(json_str));
                
                // Освобождаем предыдущие данные
                if (last_json_data) {
                    free(last_json_data);
                }
                
                // Сохраняем для возможного повторного использования
                last_json_data = json_str;
                last_json_len = strlen(json_str);
            }
        }
        
        pthread_mutex_unlock(&g_data_mutex);
        
        // Ждем указанный интервал
        struct timespec ts;
        ts.tv_sec = g_config->poll_interval_ms / 1000;
        ts.tv_nsec = (g_config->poll_interval_ms % 1000) * 1000000L;
        
        // Используем nanosleep с обработкой прерываний
        while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {
            if (!g_running) break;
        }
        
        if (!g_running) {
            log_debug("Received shutdown signal");
            break;
        }
        
        // Периодически логируем статистику
        if (loop_counter % 100 == 0) {
            log_info("Statistics: %d polls completed (%d successful, %d failed)", 
                    loop_counter, successful_polls, failed_polls);
        }
    }
    
    log_info("=== Service shutting down ===");
    log_info("Final statistics: %d polls completed (%d successful, %d failed)", 
            loop_counter, successful_polls, failed_polls);
    
    // Освобождаем JSON данные
    if (last_json_data) {
        free(last_json_data);
    }
    
    // Очистка
    cleanup();
    
    log_info("=== Service stopped ===");
    return EXIT_SUCCESS;
}

