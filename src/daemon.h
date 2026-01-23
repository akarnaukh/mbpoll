#ifndef DAEMON_H
#define DAEMON_H

#include "common.h"

// Прототипы функций
void daemonize(void);
void signal_handler(int sig);
void cleanup(void);

#endif

