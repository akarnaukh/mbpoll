# Руководство по развертыванию MBusRead

## Содержание

1. [Требования к системе](#требования-к-системе)
2. [Установка зависимостей](#установка-зависимостей)
3. [Сборка из исходного кода](#сборка-из-исходного-кода)
4. [Настройка конфигурации](#настройка-конфигурации)
5. [Установка systemd сервиса](#установка-systemd-сервиса)
6. [Первый запуск](#первый-запуск)
7. [Мониторинг и обслуживание](#мониторинг-и-обслуживание)
8. [Обновление](#обновление)
9. [Резервное копирование](#резервное-копирование)
10. [Безопасность](#безопасность)

---

## Требования к системе

### Минимальные требования

- **ОС:** Linux (Debian 10+, Ubuntu 18.04+, CentOS 7+)
- **Процессор:** 1 ядро (рекомендуется 2+)
- **ОЗУ:** 256 МБ (рекомендуется 512+ МБ)
- **Диск:** 50 МБ свободного места
- **Сеть:** Ethernet или Wi-Fi подключение

### Рекомендуемые требования

- **ОС:** Debian 11/12 или Ubuntu 20.04/22.04 LTS
- **Процессор:** 2+ ядра
- **ОЗУ:** 1 ГБ
- **Диск:** 1 ГБ SSD
- **Сеть:** Стабильное подключение к сети Modbus устройств

---

## Установка зависимостей

### Debian/Ubuntu

```bash
sudo apt update
sudo apt install -y \
    libmodbus-dev \
    libjansson-dev \
    build-essential \
    libssl-dev \
    git \
    make \
    pkg-config
```

### CentOS/RHEL

```bash
sudo yum install -y epel-release
sudo yum install -y \
    libmodbus-devel \
    jansson-devel \
    gcc \
    make \
    openssl-devel \
    git
```

### Проверка установленных зависимостей

```bash
# Проверка libmodbus
pkg-config --modversion libmodbus

# Проверка libjansson
pkg-config --modversion jansson

# Проверка компилятора
gcc --version
```

---

## Сборка из исходного кода

### Шаг 1: Клонирование репозитория

```bash
cd /opt
sudo git clone https://github.com/akarnaukh/mbpoll.git mbusread
cd mbusread
```

### Шаг 2: Сборка проекта

```bash
# Очистка и сборка
sudo make all

# Или только сборка
sudo make build
```

### Шаг 3: Проверка сборки

```bash
# Проверка исполняемого файла
ls -la build/mbusread

# Проверка systemd шаблона
ls -la build/systemd/mbusread@.service

# Проверка конфигурации
ls -la build/config/mbusread.conf
```

### Отладочная сборка (для разработки)

```bash
sudo make debug
```

---

## Настройка конфигурации

### Шаг 1: Создание директории конфигурации

```bash
sudo mkdir -p /etc/mbusread
```

### Шаг 2: Копирование шаблонов

```bash
sudo cp build/config/mbusread.conf /etc/mbusread/coold1.conf
sudo cp build/systemd/mbusread@.service /etc/systemd/system/
```

### Шаг 3: Настройка конфигурационного файла

Откройте файл конфигурации:

```bash
sudo nano /etc/mbusread/coold1.conf
```

#### Пример конфигурации для Modbus TCP устройства

```ini
# ModBus Read Service Configuration
# Modbus TCP устройство
device = 192.168.0.10:502

# Интервал между опросом в мс
poll_interval_ms = 5000

# IP адрес для прослушивания
listing_ip = 0.0.0.0


# Порт Websocket
websocket_port = 24123

# Отправка данных после опроса каждого устройства
ws_request_output = 1

# Уровень логирования
log_level = info

# Файл списка устройств
dev_list_file = /etc/mbusread/dev_list.json
```

#### Пример конфигурации для RTU устройства (последовательный порт)

```ini
# ModBus Read Service Configuration
# Serial port settings
device = /dev/ttyS1@9600,8,n,1

# Интервал между опросом в мс
poll_interval_ms = 5000

# IP адрес для прослушивания
listing_ip = 0.0.0.0


# Порт Websocket
websocket_port = 24123

# Отправка данных после опроса каждого устройства
ws_request_output = 1

# Уровень логирования
log_level = info

# Файл списка устройств
dev_list_file = /etc/mbusread/dev_list.json
```

### Шаг 4: Создание файла списка устройств

```bash
sudo nano /etc/mbusread/dev_list.json
```

#### Пример dev_list.json

```json
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
        "s": 0,
        "q": 10
      }
    },
    "12-15": {
      "3": {
        "s": 0,
        "q": 20
      }
    }
  }
}
```

**Пояснение:**
- `"10"` — устройство с адресом 10
- `"12-15"` — диапазон устройств с адресами 12, 13, 14, 15
- `"3"` — функция Modbus 3 (Holding Registers)
- `"s": 1` — стартовый адрес регистра
- `"q": 5` — количество регистров

### Шаг 5: Проверка синтаксиса JSON

```bash
python3 -m json.tool /etc/mbusread/dev_list.json > /dev/null && echo "JSON OK" || echo "JSON Error"
```

### Шаг 6: Установка прав доступа

```bash
sudo chmod 644 /etc/mbusread/coold1.conf
sudo chmod 644 /etc/mbusread/dev_list.json
sudo chown root:root /etc/mbusread/*
```

---

## Установка systemd сервиса

### Шаг 1: Установка бинарного файла

```bash
sudo make install
```

Или вручную:

```bash
sudo cp build/mbusread /usr/local/bin/
sudo chmod +x /usr/local/bin/mbusread
```

### Шаг 2: Обновление systemd

```bash
sudo systemctl daemon-reload
```

### Шаг 3: Включение сервиса

```bash
# Включение автозапуска при загрузке
sudo systemctl enable mbusread@coold1

# Запуск сервиса
sudo systemctl start mbusread@coold1

# Проверка статуса
sudo systemctl status mbusread@coold1
```

---

## Первый запуск

### Проверка работы сервиса

```bash
# Статус сервиса
sudo systemctl status mbusread@coold1

# Просмотр логов
sudo journalctl -u mbusread@coold1 -n 50 --no-pager

# Мониторинг логов в реальном времени
sudo journalctl -u mbusread@coold1 -f
```

### Ожидаемые логи при успешном запуске

```text
INFO: === Starting mbusread service ===
INFO: Configuration file: /etc/mbusread/coold1.conf
INFO: === Step 1: Loading configuration ===
INFO: Loading configuration from: /etc/mbusread/coold1.conf
INFO: Device: 192.168.0.10:502
INFO: Modbus TCP device: 192.168.0.10:502
INFO: Poll interval: 5000 ms
INFO: WebSocket server will listen on: 0.0.0.0:24123
INFO: Configuration loaded successfully
INFO: === Step 2: Loading device list ===
INFO: Total devices to load: 4
INFO: Successfully loaded 4 devices
INFO: === Step 3: Initializing Modbus connection ===
INFO: Modbus TCP connection established: 192.168.0.10:502
INFO: === Step 4: Checking Modbus connection ===
INFO: Modbus connection is OK
INFO: === Step 5: Starting WebSocket server ===
INFO: WebSocket server listening on 0.0.0.0:24123
INFO: === Service initialization complete ===
```

### Тестирование подключения

```bash
# Проверка портов
sudo netstat -tlnp | grep 24123

# Тест WebSocket подключения
wscat -c ws://localhost:24123

# Тест WebSocket подключения
wscat -c ws://localhost:24123
```

---

## Мониторинг и обслуживание

### Управление сервисом

```bash
# Запуск
sudo systemctl start mbusread@coold1

# Остановка
sudo systemctl stop mbusread@coold1

# Перезапуск
sudo systemctl restart mbusread@coold1

# Перечитывание конфигурации
sudo systemctl reload mbusread@coold1

# Статус
sudo systemctl status mbusread@coold1

# Автозапуск
sudo systemctl enable mbusread@coold1

# Отключение автозапуска
sudo systemctl disable mbusread@coold1
```

### Мониторинг логов

```bash
# Последние 100 записей
sudo journalctl -u mbusread@coold1 -n 100 --no-pager

# Логи за сегодня
sudo journalctl -u mbusread@coold1 --since today

# Логи с временными метками
sudo journalctl -u mbusread@coold1 -o short-precise

# Фильтрация по уровню
sudo journalctl -u mbusread@coold1 | grep -E "(ERROR|WARN)"

# Реальное время
sudo journalctl -u mbusread@coold1 -f
```

### Мониторинг производительности

```bash
# Использование памяти
ps aux | grep mbusread

# Количество подключений
ss -t | grep 24123 | wc -l

# Статистика системы
systemctl show mbusread@coold1 | grep -E "(MemoryCurrent|CPUTime)"
```

### Отладка проблем

```bash
# Ручной запуск для отладки
sudo systemctl stop mbusread@coold1
sudo /usr/local/bin/mbusread /etc/mbusread/coold1.conf

# Запуск под strace
strace -f -o /tmp/mbusread.strace /usr/local/bin/mbusread /etc/mbusread/coold1.conf

# Проверка утечек памяти
valgrind --leak-check=full /usr/local/bin/mbusread /etc/mbusread/coold1.conf
```

---

## Обновление

### Автоматическое обновление

```bash
cd /opt/mbusread
sudo git pull
sudo make all
sudo systemctl restart mbusread@coold1
```

### Резервное копирование перед обновлением

```bash
# Бэкап конфигурации
sudo tar -czvf /tmp/mbusread_config_backup.tar.gz /etc/mbusread/

# Бэкап текущей версии
sudo cp /usr/local/bin/mbusread /usr/local/bin/mbusread.backup
```

### Откат к предыдущей версии

```bash
# Остановка сервиса
sudo systemctl stop mbusread@coold1

# Восстановление бинарника
sudo cp /usr/local/bin/mbusread.backup /usr/local/bin/mbusread

# Запуск сервиса
sudo systemctl start mbusread@coold1
```

---

## Резервное копирование

### Что резервировать

```bash
# Конфигурация
/etc/mbusread/

# Systemd сервис
/etc/systemd/system/mbusread@.service

# Исходный код (если модифицировался)
/opt/mbusread/
```

### Скрипт резервного копирования

```bash
#!/bin/bash
# backup_mbusread.sh

BACKUP_DIR="/var/backups/mbusread"
DATE=$(date +%Y%m%d_%H%M%S)

mkdir -p $BACKUP_DIR

# Бэкап конфигурации
tar -czvf $BACKUP_DIR/config_$DATE.tar.gz /etc/mbusread/

# Бэкап бинарника
cp /usr/local/bin/mbusread $BACKUP_DIR/mbusread_$DATE

# Удаление старых бэкапов (хранить 30 дней)
find $BACKUP_DIR -name "*.tar.gz" -mtime +30 -delete
find $BACKUP_DIR -name "mbusread_*" -mtime +30 -delete

echo "Backup completed: $BACKUP_DIR"
```

### Восстановление из бэкапа

```bash
# Восстановление конфигурации
sudo tar -xzvf /var/backups/mbusread/config_20240101_120000.tar.gz -C /

# Восстановление бинарника
sudo cp /var/backups/mbusread/mbusread_20240101_120000 /usr/local/bin/mbusread
sudo chmod +x /usr/local/bin/mbusread

# Перезапуск сервиса
sudo systemctl restart mbusread@coold1
```

---

## Безопасность

### Firewall настройки

```bash
# UFW (Ubuntu)
sudo ufw allow from 192.168.0.0/24 to any port 24123 proto tcp
sudo ufw allow from 192.168.0.0/24 to any port 24123 proto tcp

# firewalld (CentOS)
sudo firewall-cmd --permanent --add-rich-rule='rule family="ipv4" source address="192.168.0.0/24" port port="24123" protocol="tcp" accept'
sudo firewall-cmd --permanent --add-rich-rule='rule family="ipv4" source address="192.168.0.0/24" port port="24123" protocol="tcp" accept'
sudo firewall-cmd --reload
```

### Ограничение доступа к конфигурации

```bash
sudo chmod 640 /etc/mbusread/*.conf
sudo chmod 640 /etc/mbusread/*.json
sudo chown root:mbusread /etc/mbusread/*
```

### Создание отдельного пользователя

```bash
# Создание пользователя
sudo useradd -r -s /bin/false mbusread

# Изменение владельца файлов
sudo chown -R mbusread:mbusread /etc/mbusread

# Изменение systemd сервиса
sudo nano /etc/systemd/system/mbusread@.service
```

Добавьте в секцию [Service]:

```ini
User=mbusread
Group=mbusread
```

### SSL/TLS для WebSocket

Для продакшн окружения рекомендуется использовать обратный прокси (nginx) с SSL:

```nginx
server {
    listen 443 ssl;
    server_name mbusread.example.com;

    ssl_certificate /etc/ssl/certs/mbusread.crt;
    ssl_certificate_key /etc/ssl/private/mbusread.key;

    location / {
        proxy_pass http://localhost:24123;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
        proxy_set_header Host $host;
    }
}
```

### Аудит безопасности

```bash
# Проверка открытых портов
sudo netstat -tlnp | grep mbusread

# Проверка прав доступа
ls -la /etc/mbusread/
ls -la /usr/local/bin/mbusread

# Проверка логов на подозрительную активность
sudo journalctl -u mbusread@coold1 | grep -i "error\|fail\|denied"
```

---

## Приложение A: Структура файлов

```
/etc/mbusread/
├── coold1.conf              # Конфигурация экземпляра coold1
├── device2.conf             # Конфигурация экземпляра device2
└── dev_list.json           # Список устройств

/usr/local/bin/
└── mbusread                # Исполняемый файл

/etc/systemd/system/
└── mbusread@.service       # Systemd шаблон сервиса

/var/log/
└── syslog                  # Системные логи (или journalctl)
```

---

## Приложение B: Команды быстрого доступа

```bash
# Alias для удобства
alias mbus-status='sudo systemctl status mbusread@coold1'
alias mbus-logs='sudo journalctl -u mbusread@coold1 -f'
alias mbus-restart='sudo systemctl restart mbusread@coold1'
alias mbus-config='sudo nano /etc/mbusread/coold1.conf'
alias mbus-devices='sudo nano /etc/mbusread/dev_list.json'
```

Добавьте в `~/.bashrc` для постоянного использования.

---

## Приложение C: Решение распространенных проблем

### Проблема: Сервис не запускается

**Диагностика:**
```bash
sudo systemctl status mbusread@coold1
sudo journalctl -u mbusread@coold1 -n 50
```

**Возможные решения:**
1. Проверить синтаксис конфигурации
2. Проверить права доступа к файлам
3. Проверить доступность Modbus устройств

### Проблема: Постоянные таймауты

**Диагностика:**
```bash
ping <modbus_device_ip>
sudo journalctl -u mbusread@coold1 | grep -i timeout
```

**Возможные решения:**
1. Увеличить `poll_interval_ms`
2. Проверить сетевое подключение
3. Проверить нагрузку на сеть

### Проблема: WebSocket не принимает подключения

**Диагностика:**
```bash
sudo netstat -tlnp | grep 24123
sudo ss -tlnp | grep 24123
```

**Возможные решения:**
1. Проверить firewall
2. Проверить настройку `websocket_port`
3. Перезапустить сервис

---

## Контакты и поддержка

- GitHub: https://github.com/akarnaukh/mbpoll
- Issues: https://github.com/akarnaukh/mbpoll/issues
- Документация: /workspace/docs/
