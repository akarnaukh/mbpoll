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

#endif

