#ifndef DAEMON_H
#define DAEMON_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>  // Добавляем для usleep/nanosleep
#include <signal.h>
#include <syslog.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <modbus/modbus.h>
#include <jansson.h>
#include <stdarg.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/select.h>  // Добавляем для select

// Уровни логирования
typedef enum {
    LOG_LEVEL_ERROR = 0,
    LOG_LEVEL_WARN = 1,
    LOG_LEVEL_INFO = 2,
    LOG_LEVEL_DEBUG = 3
} log_level_t;

// Структура для регистра
typedef struct {
    int address;
    int function;
    int start;
    int quantity;
    uint16_t *values;  // NULL если устройство недоступно
} register_range_t;

// Структура для устройства
typedef struct {
    int slave_address;
    register_range_t *ranges;
    int range_count;
    int device_available;  // 1 - доступно, 0 - недоступно
} modbus_device_t;

// Структура для конфигурации
typedef struct {
    char *device_str;
    int poll_interval_ms;
    char *listing_ip;
    int listing_port;
    log_level_t log_level;
    char *device_list_file;
    modbus_device_t *devices;
    int device_count;
    modbus_t *mb_ctx;
    int is_serial;
    char serial_port[256];
    int baud_rate;
    char tcp_ip[256];
    int tcp_port;
} config_t;

// Глобальные переменные
extern config_t *g_config;
extern pthread_mutex_t g_data_mutex;
extern int g_running;

// Функции логирования
void log_error(const char *format, ...);
void log_warn(const char *format, ...);
void log_info(const char *format, ...);
void log_debug(const char *format, ...);
void log_message(log_level_t level, const char *format, ...);

// Прототипы функций
void daemonize(void);
void signal_handler(int sig);
void cleanup(void);

#endif

