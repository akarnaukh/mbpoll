#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
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
#include <sys/select.h>
#include <sys/epoll.h>

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
    char *name;            // Имя устройства (для группировки)
    int slave_address;     // Modbus адрес устройства
    register_range_t *ranges;
    int range_count;
    int device_available;  // 1 - доступно, 0 - недоступно
} modbus_device_t;

// Структура для команды записи Modbus
typedef struct {
    int value;
    int function_code;  // 5, 6, 15, 16
    int unit_id;
    int address;
    int quantity;
    uint16_t *values;   // Для функций 15/16 - массив значений
} modbus_write_command_t;

// Структура для WebSocket клиента
typedef struct websocket_client {
    int fd;
    struct websocket_client *next;
    time_t last_active;
} websocket_client_t;

// Структура для конфигурации
typedef struct config_s {
    char *device_str;
    int poll_interval_ms;
    char *listing_ip;
    int listing_port;
    int websocket_port;  // Порт для WebSocket
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
    
    // WebSocket
    websocket_client_t *ws_clients;
    pthread_mutex_t ws_mutex;
    int ws_server_fd;
    
    // Вывод каждого запроса в WebSocket
    int ws_request_output;  // 1 - выводить каждый запрос, 0 - только полный результат
    
    // Очередь команд записи
    modbus_write_command_t *write_commands;
    int write_command_count;
    pthread_mutex_t write_queue_mutex;
    
    // Флаг приостановки опроса
    int pause_polling;
} config_t;

// Глобальные переменные (объявлены в daemon.c)
extern config_t *g_config;
extern pthread_mutex_t g_data_mutex;
extern int g_running;

// Функции логирования (реализованы в daemon.c)
void log_error(const char *format, ...);
void log_warn(const char *format, ...);
void log_info(const char *format, ...);
void log_debug(const char *format, ...);
void log_message(log_level_t level, const char *format, ...);

#endif

