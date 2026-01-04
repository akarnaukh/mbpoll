#include "config.h"
#include "daemon.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

int load_config(const char *config_file, config_t *config) {
    log_info("Loading configuration from: %s", config_file);
    
    FILE *fp = fopen(config_file, "r");
    if (!fp) {
        log_error("Cannot open config file: %s", config_file);
        return -1;
    }
    
    char line[256];
    int line_num = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        line_num++;
        
        // Пропускаем комментарии и пустые строки
        if (line[0] == '#' || line[0] == '\n') {
            log_debug("Line %d: Comment or empty line", line_num);
            continue;
        }
        
        // Удаляем символ новой строки
        line[strcspn(line, "\n")] = '\0';
        
        // Удаляем лишние пробелы в начале
        char *trimmed = line;
        while (*trimmed == ' ') trimmed++;
        
        // Пропускаем пустые строки после удаления пробелов
        if (trimmed[0] == '\0') {
            continue;
        }
        
        char *key = strtok(trimmed, " =");
        char *value = strtok(NULL, "=");
        
        // Если value начинается с пробела, пропускаем его
        if (value && *value == ' ') {
            value++;
        }
        
        if (!key || !value) {
            log_warn("Line %d: Invalid format, skipping", line_num);
            continue;
        }
        
        log_debug("Line %d: Parsed key='%s', value='%s'", line_num, key, value);
        
        if (strcmp(key, "device") == 0) {
            config->device_str = strdup(value);
            log_debug("  Set device string: %s", value);
        } else if (strcmp(key, "poll_interval_ms") == 0) {
            config->poll_interval_ms = atoi(value);
            log_debug("  Set poll interval: %d ms", config->poll_interval_ms);
        } else if (strcmp(key, "listing_ip") == 0) {
            config->listing_ip = strdup(value);
            log_debug("  Set listing IP: %s", value);
        } else if (strcmp(key, "listing_port") == 0) {
            config->listing_port = atoi(value);
            log_debug("  Set listing port: %d", config->listing_port);
        } else if (strcmp(key, "log_level") == 0) {
            if (strcmp(value, "debug") == 0) {
                config->log_level = LOG_LEVEL_DEBUG;
                log_debug("  Set log level: DEBUG");
            } else if (strcmp(value, "info") == 0) {
                config->log_level = LOG_LEVEL_INFO;
                log_debug("  Set log level: INFO");
            } else if (strcmp(value, "warn") == 0) {
                config->log_level = LOG_LEVEL_WARN;
                log_debug("  Set log level: WARN");
            } else if (strcmp(value, "error") == 0) {
                config->log_level = LOG_LEVEL_ERROR;
                log_debug("  Set log level: ERROR");
            } else {
                log_warn("Unknown log level: %s, using default 'error'", value);
                config->log_level = LOG_LEVEL_ERROR;
            }
        } else if (strcmp(key, "dev_list_file") == 0) {
            config->device_list_file = strdup(value);
            log_debug("  Set device list file: %s", value);
	} else if (strcmp(key, "connection_timeout_ms") == 0) {
            int timeout_ms = atoi(value);
            // Сохраняем где-нибудь в конфиге
            // Пока просто логируем
            log_debug("  Set connection timeout: %d ms", timeout_ms);
        } else {
            log_warn("Unknown configuration key: %s", key);
        }
    }
    
    fclose(fp);
    
    // Проверяем обязательные параметры
    int config_valid = 1;
    
    if (!config->device_str) {
        log_error("Missing 'device' in configuration");
        config_valid = 0;
    } else {
        log_info("Device: %s", config->device_str);
    }
    
    if (!config->device_list_file) {
        log_error("Missing 'dev_list_file' in configuration");
        config_valid = 0;
    } else {
        log_info("Device list file: %s", config->device_list_file);
    }
    
    if (!config_valid) {
        return -1;
    }
    
    // Устанавливаем значения по умолчанию для необязательных параметров
    if (!config->listing_ip) {
        config->listing_ip = strdup("0.0.0.0");
        log_info("Using default listing IP: 0.0.0.0");
    }
    
    if (config->poll_interval_ms <= 0) {
        config->poll_interval_ms = 1000;
        log_info("Using default poll interval: 1000 ms");
    }
    
    if (config->listing_port <= 0) {
        config->listing_port = 24122;
        log_info("Using default listing port: 24122");
    }
    
    // Парсим строку устройства
    if (strstr(config->device_str, "/dev/") != NULL) {
        config->is_serial = 1;
        if (sscanf(config->device_str, "%255[^@]@%d", 
                   config->serial_port, &config->baud_rate) == 2) {
            log_info("Serial device: %s@%d", config->serial_port, config->baud_rate);
        } else {
            log_error("Invalid serial device format: %s", config->device_str);
            return -1;
        }
    } else {
        config->is_serial = 0;
        if (sscanf(config->device_str, "%255[^:]:%d", 
                   config->tcp_ip, &config->tcp_port) == 2) {
            log_info("TCP device: %s:%d", config->tcp_ip, config->tcp_port);
        } else {
            log_error("Invalid TCP device format: %s", config->device_str);
            return -1;
        }
    }
    
    log_info("Poll interval: %d ms", config->poll_interval_ms);
    log_info("TCP server will listen on: %s:%d", config->listing_ip, config->listing_port);
    log_info("Log level: %s", 
             config->log_level == LOG_LEVEL_DEBUG ? "DEBUG" :
             config->log_level == LOG_LEVEL_INFO ? "INFO" :
             config->log_level == LOG_LEVEL_WARN ? "WARN" : "ERROR");
    
    log_info("Configuration loaded successfully");
    return 0;
}

