#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include "common.h"

// WebSocket функции
int init_websocket_server(config_t *config);
void *websocket_server_thread(void *arg);
void broadcast_to_websockets(config_t *config, const char *data, size_t len);
void close_websocket_client(websocket_client_t *client);

#endif

