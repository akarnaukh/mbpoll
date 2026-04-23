#ifndef MODBUS_CLIENT_H
#define MODBUS_CLIENT_H

#include "common.h"

int init_modbus_connection(config_t *config);
int reinit_modbus_connection(config_t *config);
int check_modbus_connection(config_t *config);
void poll_all_devices(config_t *config);
int check_and_execute_write_commands(config_t *config);

#endif

