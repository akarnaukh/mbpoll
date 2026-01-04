#include "daemon.h"
#include "config.h"
#include "modbus_client.h"
#include "device_list.h"
#include "tcp_server.h"
#ifndef VERSION_STRING
#define VERSION_STRING "unknown"
#endif
int main(int argc, char *argv[]) {
    int run_foreground = 0;
    
    // Проверяем аргументы
    if (argc == 3 && strcmp(argv[1], "--foreground") == 0) {
        run_foreground = 1;
        argv[1] = argv[2]; // Сдвигаем аргументы
    } else if (argc != 2) {
	fprintf(stderr, "=== Modbus reader Service. Version:%s ===\n",VERSION_STRING);
        fprintf(stderr, "Usage: %s [--foreground] <config_file>\n", argv[0]);
        return EXIT_FAILURE;
    }
    
    // Открываем syslog
    openlog("mbusread", LOG_PID | LOG_CONS, LOG_DAEMON);
//    log_info("=== Starting mbusread service ===");
    if (run_foreground) {
        log_info("=== Starting mbusread in foreground mode ===");
        // В режиме foreground также выводим в stderr
        fprintf(stderr, "=== Starting mbusread in foreground mode ===\n");
    } else {
        log_info("=== Starting mbusread service ===");
    }

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
    
    // === ШАГ 5: Запуск TCP сервера ===
    log_info("=== Step 5: Starting TCP server ===");
    pthread_t tcp_thread;
    if (pthread_create(&tcp_thread, NULL, tcp_server_thread, g_config) != 0) {
        log_error("Failed to create TCP server thread");
        cleanup();
        return EXIT_FAILURE;
    }
    
    // Отделяем поток, чтобы он завершился автоматически при выходе
    pthread_detach(tcp_thread);
    
    log_info("=== Service initialization complete ===");
    log_info("Poll interval: %d ms", g_config->poll_interval_ms);
    log_info("TCP server listening on %s:%d", g_config->listing_ip, g_config->listing_port);
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
    
    while (g_running) {
        loop_counter++;
        connection_check_counter++;
        
        if (loop_counter % 10 == 0) {
            log_debug("=== Polling cycle %d ===", loop_counter);
        }
        
        pthread_mutex_lock(&g_data_mutex);
        
        // Проверяем доступность соединения не каждый раз, а например, каждый 5-й цикл
        // или если была предыдущая ошибка
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
            connection_check_counter = 0; // Сбрасываем счетчик
        }
        
        // Опрашиваем все устройства
        poll_all_devices(g_config);
        successful_polls++;
        
        pthread_mutex_unlock(&g_data_mutex);
        
        // Ждем указанный интервал
        struct timespec ts;
        ts.tv_sec = g_config->poll_interval_ms / 1000;
        ts.tv_nsec = (g_config->poll_interval_ms % 1000) * 1000000L;
        
        log_debug("Sleeping for %d ms...", g_config->poll_interval_ms);
        
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
    
    // Очистка
    cleanup();
    
    log_info("=== Service stopped ===");
    return EXIT_SUCCESS;
}

