#include "daemon.h"
#include "config.h"
#include "modbus_client.h"
#include "device_list.h"
#include "tcp_server.h"
#include "websocket.h"
#include <stdarg.h>

config_t *g_config = NULL;
pthread_mutex_t g_data_mutex = PTHREAD_MUTEX_INITIALIZER;
int g_running = 1;

// Функция логирования
void log_message(log_level_t level, const char *format, ...) {
    va_list args;
    va_start(args, format);
    
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);
    
    // Всегда пишем в syslog
    int syslog_level;
    switch (level) {
        case LOG_LEVEL_DEBUG: syslog_level = LOG_DEBUG; break;
        case LOG_LEVEL_INFO: syslog_level = LOG_INFO; break;
        case LOG_LEVEL_WARN: syslog_level = LOG_WARNING; break;
        case LOG_LEVEL_ERROR: syslog_level = LOG_ERR; break;
        default: syslog_level = LOG_INFO;
    }
    
    syslog(syslog_level, "%s", buffer);
    
    va_end(args);
}

// Упрощенные функции логирования
void log_error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);
    log_message(LOG_LEVEL_ERROR, "ERROR: %s", buffer);
    va_end(args);
}

void log_warn(const char *format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);
    log_message(LOG_LEVEL_WARN, "WARN: %s", buffer);
    va_end(args);
}

void log_info(const char *format, ...) {
    va_list args;
    va_start(args, format);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), format, args);
    log_message(LOG_LEVEL_INFO, "INFO: %s", buffer);
    va_end(args);
}

void log_debug(const char *format, ...) {
    if (g_config && g_config->log_level >= LOG_LEVEL_DEBUG) {
        va_list args;
        va_start(args, format);
        char buffer[1024];
        vsnprintf(buffer, sizeof(buffer), format, args);
        log_message(LOG_LEVEL_DEBUG, "DEBUG: %s", buffer);
        va_end(args);
    }
}

// Пустая функция демонизации (systemd делает это за нас)
void daemonize(void) {
    log_info("Running under systemd, no daemonization needed");
}

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        log_info("Received signal %d, shutting down...", sig);
        g_running = 0;
    }
}

void cleanup(void) {
    log_info("Cleaning up resources...");
    
    if (g_config) {
        // Освобождаем память для устройств
        if (g_config->devices) {
            for (int i = 0; i < g_config->device_count; i++) {
                if (g_config->devices[i].ranges) {
                    for (int j = 0; j < g_config->devices[i].range_count; j++) {
                        free(g_config->devices[i].ranges[j].values);
                    }
                    free(g_config->devices[i].ranges);
                }
            }
            free(g_config->devices);
        }
        
        // Закрываем Modbus соединение
        if (g_config->mb_ctx) {
            log_info("Closing Modbus connection...");
            modbus_close(g_config->mb_ctx);
            modbus_free(g_config->mb_ctx);
        }
        
        // Закрываем WebSocket соединения
        if (g_config->ws_clients) {
            log_info("Closing WebSocket connections...");
            websocket_client_t *client = g_config->ws_clients;
            while (client) {
                websocket_client_t *next = client->next;
                close_websocket_client(client);
                client = next;
            }
            g_config->ws_clients = NULL;
        }
        
        // Закрываем WebSocket сервер
        if (g_config->ws_server_fd > 0) {
            close(g_config->ws_server_fd);
        }
        
        pthread_mutex_destroy(&g_config->ws_mutex);
        
        free(g_config->device_str);
        free(g_config->listing_ip);
        free(g_config->device_list_file);
        free(g_config);
        g_config = NULL;
    }
    
    pthread_mutex_destroy(&g_data_mutex);
    closelog();
}

