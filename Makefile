# Makefile для mbusread сервиса
VERSION = 0.1
CC = gcc

# Проверяем наличие функции modbus_set_tcp_keepalive
HAVE_TCP_KEEPALIVE := $(shell echo "\#include <modbus/modbus.h>\nint main() { modbus_set_tcp_keepalive(NULL, 0); return 0; }" | $(CC) -x c - -lmodbus -o /dev/null 2>/dev/null && echo "-DHAVE_TCP_KEEPALIVE")

CFLAGS = -Wall -Wextra -O2 -std=c11 -DVERSION_STRING=\"$(VERSION)\" -D_POSIX_C_SOURCE=200809L $(HAVE_TCP_KEEPALIVE)
LDFLAGS = -lmodbus -ljansson -pthread
DEBUG_CFLAGS = -DDEBUG -g -O0

SRC_DIR = src
BUILD_DIR = build
BIN_NAME = mbusread
INSTALL_DIR = /usr/local/bin
SYSTEMD_DIR = /etc/systemd/system
CONFIG_DIR = /etc/mbusread
LOG_DIR = /var/log/mbusread

# Поиск исходных файлов
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

.PHONY: all clean install uninstall template debug test monitor

all: clean build template

build: $(BUILD_DIR)/$(BIN_NAME)

$(BUILD_DIR)/$(BIN_NAME): $(OBJS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(BUILD_DIR)/*
	rm -f $(BIN_NAME)

template:
	@mkdir -p $(BUILD_DIR)/systemd
	@mkdir -p $(BUILD_DIR)/config
	@echo "# ModBus Read V$(VERSION)" > $(BUILD_DIR)/config/mbusread.conf
	@echo "# Serial port settings" >> $(BUILD_DIR)/config/mbusread.conf
	@echo "# device = /dev/ttyS1@9600" >> $(BUILD_DIR)/config/mbusread.conf
	@echo "# or TCP device settings" >> $(BUILD_DIR)/config/mbusread.conf
	@echo "device = 192.168.0.10:502" >> $(BUILD_DIR)/config/mbusread.conf
	@echo "" >> $(BUILD_DIR)/config/mbusread.conf
	@echo "# Интервал между опросом в мс" >> $(BUILD_DIR)/config/mbusread.conf
	@echo "poll_interval_ms = 5000" >> $(BUILD_DIR)/config/mbusread.conf
	@echo "" >> $(BUILD_DIR)/config/mbusread.conf
	@echo "listing_ip = 0.0.0.0" >> $(BUILD_DIR)/config/mbusread.conf
	@echo "listing_port = 24122" >> $(BUILD_DIR)/config/mbusread.conf
	@echo "" >> $(BUILD_DIR)/config/mbusread.conf
	@echo "log_level = info" >> $(BUILD_DIR)/config/mbusread.conf
	@echo "# Доступны - debug: Все пишем в лог, включая данные регистров," >> $(BUILD_DIR)/config/mbusread.conf
	@echo "# info: События modbus, warn - timeout при чтении," >> $(BUILD_DIR)/config/mbusread.conf
	@echo "# error - только критические ошибки" >> $(BUILD_DIR)/config/mbusread.conf
	@echo "" >> $(BUILD_DIR)/config/mbusread.conf
	@echo "# Файл списка устройств" >> $(BUILD_DIR)/config/mbusread.conf
	@echo "dev_list_file = /etc/mbusread/dev_list.json" >> $(BUILD_DIR)/config/mbusread.conf
	
	@echo "[Unit]" > $(BUILD_DIR)/systemd/mbusread@.service
	@echo "Description=Modbus Read Service %I" >> $(BUILD_DIR)/systemd/mbusread@.service
	@echo "After=network.target" >> $(BUILD_DIR)/systemd/mbusread@.service
	@echo "" >> $(BUILD_DIR)/systemd/mbusread@.service
	@echo "[Service]" >> $(BUILD_DIR)/systemd/mbusread@.service
	@echo "Type=simple" >> $(BUILD_DIR)/systemd/mbusread@.service
	@echo "ExecStart=$(INSTALL_DIR)/$(BIN_NAME) /etc/mbusread/%i.conf" >> $(BUILD_DIR)/systemd/mbusread@.service
	@echo "Restart=always" >> $(BUILD_DIR)/systemd/mbusread@.service
	@echo "RestartSec=5" >> $(BUILD_DIR)/systemd/mbusread@.service
	@echo "SyslogIdentifier=mbusread-%i" >> $(BUILD_DIR)/systemd/mbusread@.service
	@echo "StandardOutput=journal" >> $(BUILD_DIR)/systemd/mbusread@.service
	@echo "StandardError=journal" >> $(BUILD_DIR)/systemd/mbusread@.service
	@echo "" >> $(BUILD_DIR)/systemd/mbusread@.service
	@echo "[Install]" >> $(BUILD_DIR)/systemd/mbusread@.service
	@echo "WantedBy=multi-user.target" >> $(BUILD_DIR)/systemd/mbusread@.service

debug: clean
	$(CC) $(CFLAGS) $(DEBUG_CFLAGS) -o $(BUILD_DIR)/$(BIN_NAME) $(SRCS) $(LDFLAGS)
	@make template

install:
	@mkdir -p $(INSTALL_DIR)
	cp $(BUILD_DIR)/$(BIN_NAME) $(INSTALL_DIR)/
	@mkdir -p $(CONFIG_DIR)
	cp $(BUILD_DIR)/config/mbusread.conf $(CONFIG_DIR)/
	cp $(BUILD_DIR)/systemd/mbusread@.service $(SYSTEMD_DIR)/
	@mkdir -p $(LOG_DIR)
	systemctl daemon-reload

uninstall:
	rm -f $(INSTALL_DIR)/$(BIN_NAME)
	rm -f $(SYSTEMD_DIR)/mbusread@.service
	systemctl daemon-reload

# Проверяем версию libmodbus
check-libmodbus:
	@echo "Checking libmodbus version..."
	@pkg-config --modversion libmodbus 2>/dev/null || echo "libmodbus version unknown"
	@echo "HAVE_TCP_KEEPALIVE flag: $(HAVE_TCP_KEEPALIVE)"

test:
	@echo "=== Running quick test ==="
	@./install_and_test.sh 2>/dev/null || echo "Test script not found, please create it first"

monitor:
	@echo "=== Monitoring service ==="
	sudo journalctl -u mbusread@coold1 -f

client-test:
	@echo "=== Testing TCP client ==="
	@./test_tcp_client.sh 2>/dev/null || echo "Test client script not found, please create it first"

help:
	@echo "Available targets:"
	@echo "  all               - Clean, build and create templates"
	@echo "  clean             - Remove build files"
	@echo "  debug             - Build with debug symbols"
	@echo "  install           - Install to system"
	@echo "  uninstall         - Remove from system"
	@echo "  check-libmodbus   - Check libmodbus version and features"
	@echo "  test              - Run installation test"
	@echo "  monitor           - Monitor service logs"
	@echo "  client-test       - Test TCP client access"
	@echo "  help              - Show this help"

