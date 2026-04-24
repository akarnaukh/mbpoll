#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include "common.h"

// WebSocket функции
int init_websocket_server(config_t *config);
void *websocket_server_thread(void *arg);
void broadcast_to_websockets(config_t *config, const char *data, size_t len);
void close_websocket_client(websocket_client_t *client);

// Функция для отправки отдельного запроса в WebSocket
void send_request_to_websockets(config_t *config, int device_address, int function, 
                                 int start, int quantity, int success, uint16_t *values);

// Функция для отправки данных устройства в WebSocket (для ws_request_output)
void send_device_to_websockets(config_t *config, modbus_device_t *device);

// Функция для обработки входящих команд записи Modbus
void process_modbus_write_command(config_t *config, const char *json_data);

// Функция для отправки результата записи в WebSocket
void send_write_result_to_websocket(config_t *config, int success, const char *error_msg);
#endif

