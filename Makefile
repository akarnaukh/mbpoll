# Makefile для mbusread сервиса
VERSION = 0.1
CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c11 -D_POSIX_C_SOURCE=200809L
LDFLAGS = -lmodbus -ljansson -pthread -lssl -lcrypto
DEBUG_CFLAGS = -DDEBUG -g -O0

SRC_DIR = src
BUILD_DIR = build
BIN_NAME = mbusread
INSTALL_DIR = /usr/local/bin
SYSTEMD_DIR = /etc/systemd/system
CONFIG_DIR = /etc/mbusread

# Поиск исходных файлов
SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

.PHONY: all clean install uninstall template debug

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
	@echo "websocket_port = 24123" >> $(BUILD_DIR)/config/mbusread.conf
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
	systemctl daemon-reload

uninstall:
	rm -f $(INSTALL_DIR)/$(BIN_NAME)
	rm -f $(SYSTEMD_DIR)/mbusread@.service
	systemctl daemon-reload
