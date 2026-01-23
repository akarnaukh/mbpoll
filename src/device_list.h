#ifndef DEVICE_LIST_H
#define DEVICE_LIST_H

#include "common.h"

int parse_address_range(const char *str, int *start, int *end);
int load_device_list(config_t *config);

#endif

