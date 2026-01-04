#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include "daemon.h"
#include <jansson.h>

void *tcp_server_thread(void *arg);
void send_data_to_client(int client_fd, config_t *config);

#endif

