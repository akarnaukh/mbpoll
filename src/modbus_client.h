#ifndef MODBUS_CLIENT_H
#define MODBUS_CLIENT_H

#include "common.h"

int init_modbus_connection(config_t *config);
int reinit_modbus_connection(config_t *config);
int check_modbus_connection(config_t *config);
void poll_all_devices(config_t *config);
int check_and_execute_write_commands(config_t *config);
int execute_modbus_write(config_t *config, modbus_write_command_t *cmd);
void execute_modbus_write_immediate(config_t *config, modbus_write_command_t *cmd);

#endif

