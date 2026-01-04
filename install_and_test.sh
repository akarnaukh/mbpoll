#!/bin/bash

echo "=== Installing and testing mbusread service ==="

# Останавливаем сервис если запущен
sudo systemctl stop mbusread@coold1 2>/dev/null

# Собираем проект
echo "Building project..."
make clean
make all

# Устанавливаем
echo "Installing..."
sudo make install

# Создаем тестовую конфигурацию
echo "Creating test configuration..."
sudo mkdir -p /etc/mbusread

sudo tee /etc/mbusread/coold1.conf > /dev/null << CONF
# ModBus Read Service Configuration
device = 10.0.140.95:28485
poll_interval_ms = 5000
listing_ip = 0.0.0.0
listing_port = 24122
log_level = debug
dev_list_file = /etc/mbusread/dev_list.json
CONF

# Создаем тестовый список устройств
echo "Creating test device list..."
sudo tee /etc/mbusread/dev_list.json > /dev/null << JSON
{
  "devices": {
    "10": {
      "3": {
        "s": 1,
        "q": 5
      },
      "4": {
        "s": 22,
        "q": 3
      }
    },
    "11": {
      "3": {
        "s": 1,
        "q": 5
      },
      "4": {
        "s": 22,
        "q": 3
      }
    }
  }
}
JSON

echo "=== Configuration files ==="
echo "Config: /etc/mbusread/coold1.conf"
sudo cat /etc/mbusread/coold1.conf
echo ""
echo "Device list: /etc/mbusread/dev_list.json"
sudo cat /etc/mbusread/dev_list.json
echo ""

# Перезагружаем systemd
sudo systemctl daemon-reload

# Включаем автозапуск
sudo systemctl enable mbusread@coold1

# Запускаем сервис
echo "Starting service..."
sudo systemctl start mbusread@coold1

# Ждем и проверяем статус
sleep 2
echo ""
echo "=== Service status ==="
sudo systemctl status mbusread@coold1 --no-pager

echo ""
echo "=== Checking logs ==="
sudo journalctl -u mbusread@coold1 -n 20 --no-pager

echo ""
echo "=== Testing TCP access ==="
echo "To test TCP data access, run:"
echo "  nc 0.0.0.0 24122 | head -c 1000"
echo ""
echo "Or use this command to see the data structure:"
echo "  (echo -n && sleep 0.1) | nc 0.0.0.0 24122 | python3 -m json.tool"
