# Архитектура MBusRead

## Обзор системы

MBusRead — это демонизированный сервис для опроса Modbus устройств (TCP/RTU) с передачей данных через WebSocket.

## Компоненты системы

```
┌─────────────────────────────────────────────────────────────┐
│                    Systemd Service                          │
│              (mbusread@instance.service)                    │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                   MBusRead Daemon                           │
│                      (main.c)                               │
│  ┌──────────────────────────────────────────────────────┐   │
│  │  Initialization Sequence                             │   │
│  │  1. Load Configuration (config.c)                    │   │
│  │  2. Load Device List (device_list.c)                 │   │
│  │  3. Initialize Modbus Client (modbus_client.c)       │   │
│  │  4. Start WebSocket Server (websocket.c)             │   │
│  │  5. Start Polling Loop                               │   │
│  └──────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
          │                        │                    │
          ▼                        ▼                    ▼
┌──────────────────┐    ┌──────────────────┐  ┌──────────────────┐
│  Config File     │    │  Modbus Devices  │  │  WebSocket       │
│  /etc/mbusread/  │    │  WebSocket       │  │  Clients         │
│  *.conf          │    │  Devices         │  │  (JSON data)     │
└──────────────────┘    └──────────────────┘  └──────────────────┘
```

## Модульная структура

### 1. main.c — Главный модуль

**Ответственность:** Инициализация и координация всех компонентов

**Основные функции:**
- `main()` — Точка входа, последовательная инициализация
- `cleanup()` — Освобождение ресурсов при завершении

**Поток выполнения:**
```c
main()
├── Parse command line arguments
├── Open syslog for logging
├── Initialize config structure
├── Step 1: load_config()
├── Step 2: load_device_list()
├── Step 3: init_modbus()
├── Step 4: check_modbus_connection()
├── Step 5: start_websocket_server()
└── Enter polling loop
    ├── For each device in list
    │   ├── poll_modbus_device()
    │   ├── broadcast_to_websockets()
    │   └── handle_write_commands()
    └── Sleep for poll_interval_ms
```

### 2. config.c — Конфигурация

**Ответственность:** Парсинг конфигурационного файла

**Структуры данных:**
```c
typedef struct {
    char device[256];           // Modbus устройство (TCP/IP или serial)
    int poll_interval_ms;       // Интервал опроса
    char listing_ip;           // IP для прослушивания WebSocket
    int websocket_port;         // Порт WebSocket
    int ws_request_output;      // Режим вывода WebSocket
    int log_level;              // Уровень логирования
    char dev_list_file[512];    // Путь к файлу списка устройств
    write_command_t *write_commands;  // Очередь команд на запись
    int write_command_count;    // Количество команд на запись
    pthread_mutex_t write_queue_mutex;  // Мьютекс очереди записи
    int pause_polling;          // Флаг паузы опроса
} config_t;
```

**Функции:**
- `load_config(const char *filename, config_t *config)` — Загрузка и парсинг
- `parse_log_level(const char *level)` — Парсинг уровня логирования

### 3. device_list.c — Список устройств

**Ответственность:** Парсинг JSON файла со списком Modbus устройств

**Структуры данных:**
```c
typedef struct {
    int function;               // Функция Modbus (2, 3, 4, etc.)
    int start;                  // Стартовый адрес
    int quantity;               // Количество регистров
    uint16_t *values;           // Значения регистров
    int available;              // Статус доступности
} modbus_range_t;

typedef struct {
    int address;                // Адрес устройства Modbus
    modbus_range_t **ranges;    // Массив диапазонов регистров
    int range_count;            // Количество диапазонов
    int available;              // Статус доступности устройства
} modbus_device_t;

typedef struct {
    modbus_device_t **devices;  // Массив устройств
    int device_count;           // Количество устройств
} device_list_t;
```

**Формат JSON:**
```json
{
  "devices": {
    "10": {
      "3": { "s": 1, "q": 5 },
      "4": { "s": 22, "q": 3 }
    },
    "11-15": {
      "3": { "s": 0, "q": 10 }
    }
  }
}
```

**Функции:**
- `load_device_list(config_t *config)` — Загрузка из файла
- `parse_device_range()` — Парсинг диапазона адресов (напр. "11-15")
- `free_device_list()` — Освобождение памяти

### 4. modbus_client.c — Modbus клиент

**Ответственность:** Взаимодействие с Modbus устройствами

**Функции:**
- `init_modbus(config_t *config)` — Инициализация соединения
- `check_modbus_connection()` — Проверка доступности
- `poll_modbus_device()` — Опрос одного устройства
- `reconnect_modbus()` — Переподключение при ошибке
- `close_modbus()` — Закрытие соединения

**Поддерживаемые функции Modbus:**
- FC 2: Input Bits (дискретные входы)
- FC 3: Holding Registers (регистры хранения)
- FC 4: Input Registers (входные регистры)
- FC 5: Write Single Coil
- FC 6: Write Single Register
- FC 15: Write Multiple Coils
- FC 16: Write Multiple Registers

**Обработка ошибок:**
```c
switch (error_code) {
    case CONNECTION_TIMEOUT:
        // Не критично, помечаем устройство как недоступное
        device->available = false;
        break;
    case CRC_ERROR:
        // Критично, пробуем переподключиться
        reconnect_modbus();
        break;
}
```

### 5. websocket.c — WebSocket сервер

**Ответственность:** Передача данных клиентам через WebSocket

**Структуры данных:**
```c
typedef struct websocket_client {
    int socket_fd;              // Сокет клиента
    char ip_address[32];        // IP адрес клиента
    struct websocket_client *next;  // Следующий клиент в списке
} websocket_client_t;
```

**Функции:**
- `init_websocket_server()` — Инициализация сервера
- `websocket_server_thread()` — Поток обработки подключений
- `broadcast_to_websockets()` — Рассылка данных всем клиентам
- `send_request_to_websockets()` — Отправка данных после опроса устройства
- `send_device_to_websockets()` — Отправка полных данных устройства
- `process_modbus_write_command()` — Обработка команд на запись
- `send_write_result_to_websocket()` — Отправка результата записи

**Формат сообщений:**

Входящие (Write Command):
```json
{
    "value": [5000, 2300, 4000, 7300],
    "fc": 16,
    "unitid": 210,
    "address": 5,
    "quantity": 4
}
```

Исходящие (Polling Data):
```json
{
    "devices": [
        {
            "address": 10,
            "available": true,
            "registers": [
                {
                    "function": 3,
                    "start": 1,
                    "quantity": 5,
                    "values": [21, 3, 3, 0, 0]
                }
            ]
        }
    ],
    "timestamp": 1672156800
}
```

### 6. daemon.c — Демонизация и логирование

**Ответственность:** Логирование через syslog

**Уровни логирования:**
- `LOG_LEVEL_DEBUG` — Полная отладочная информация
- `LOG_LEVEL_INFO` — Основные события
- `LOG_LEVEL_WARN` — Предупреждения
- `LOG_LEVEL_ERROR` — Критические ошибки

**Функции:**
- `openlog()` — Открытие syslog
- `log_debug/info/warn/error()` — Функции логирования

## Поток данных

```
┌─────────────┐
│ Config File │
└──────┬──────┘
       │
       ▼
┌─────────────────────────────────────────┐
│         Initialization Phase            │
│  1. Load configuration                  │
│  2. Parse device list JSON              │
│  3. Allocate memory for devices         │
│  4. Initialize Modbus connection        │
│  5. Start WebSocket server thread       │
└──────────────┬──────────────────────────┘
               │
               ▼
┌─────────────────────────────────────────┐
│          Polling Loop (infinite)        │
│                                         │
│  For each device in device_list:        │
│    ├─► Check if device is available     │
│    ├─► Send Modbus request (FC 2/3/4)   │
│    ├─► Read response registers          │
│    ├─► Update device->values            │
│    ├─► If ws_request_output=1:          │
│    │   └─► Broadcast to WebSocket       │
│    └─► Check for write commands         │
│                                         │
│  After all devices polled:              │
│    └─► Broadcast full state to WS       │
│                                         │
│  Sleep poll_interval_ms                 │
└─────────────────────────────────────────┘
```

## Управление памятью

### Выделение памяти
1. **При старте:**
   - `config_t` — структура конфигурации
   - `device_list_t` — список устройств
   - `modbus_device_t[]` — массив устройств
   - `modbus_range_t[]` — диапазоны регистров для каждого устройства

2. **Динамически:**
   - `values[]` — массив значений регистров (при первом успешном чтении)
   - `websocket_client_t` — структуры подключенных клиентов
   - `write_command_t` — команды на запись

### Освобождение памяти
1. **При недоступности устройства:**
   ```c
   free(device->ranges[i]->values);
   device->ranges[i]->values = NULL;
   ```

2. **При завершении работы:**
   ```c
   cleanup()
   ├── free_device_list()
   ├── close_modbus()
   ├── close_websocket_server()
   └── free(g_config)
   ```

## Многопоточность

### Потоки
1. **Main Thread** — Цикл опроса Modbus устройств
2. **WebSocket Thread** — Обработка подключений клиентов

### Синхронизация
```c
// Мьютекс для очереди команд на запись
pthread_mutex_t write_queue_mutex;

// Пример использования:
pthread_mutex_lock(&g_config->write_queue_mutex);
// ... работа с очередью записи ...
pthread_mutex_unlock(&g_config->write_queue_mutex);
```

## Отказоустойчивость

### Стратегии восстановления

1. **Потеря соединения Modbus:**
   - Логируется предупреждение
   - Закрывается текущее соединение
   - Пауза 2 секунды
   - Попытка переподключения
   - Экспоненциальная задержка при неудаче (до 30 сек)

2. **Недоступное устройство:**
   - Помечается как `available = false`
   - Последующие функции для устройства пропускаются
   - В WebSocket отправляется `"available": false`

3. **Ошибка парсинга JSON:**
   - Критическая ошибка
   - Сервис завершает работу

4. **WebSocket клиент отключился:**
   - Клиент удаляется из списка
   - Сокет закрывается
   - Ресурсы освобождаются

## Systemd интеграция

### Шаблон сервиса
```ini
[Unit]
Description=Modbus Read Service %I
After=network.target

[Service]
Type=simple
ExecStart=/usr/local/bin/mbusread /etc/mbusread/%i.conf
Restart=always
RestartSec=5
SyslogIdentifier=mbusread-%I
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
```

### Множественные экземпляры
```bash
# Запуск нескольких независимых экземпляров
systemctl start mbusread@coold1
systemctl start mbusread@device2
systemctl start mbusread@substation3
```

## Безопасность

### Рекомендации
1. Ограничить доступ к портам через firewall
2. Использовать отдельные конфигурации для разных сетей
3. Регулярно обновлять зависимости (libmodbus, libjansson)
4. Мониторить логи на предмет аномалий

## Производительность

### Оптимизации
- Пуллинг устройств в одном потоке
- Минимальное выделение памяти в цикле опроса
- Буферизация WebSocket сообщений
- Асинхронная обработка клиентов

### Ограничения
- Максимальное количество устройств зависит от памяти
- Время цикла опроса = Σ(время опроса устройства) + задержки
- WebSocket广播 может быть узким местом при многих клиентах

## Расширяемость

### Добавление новых функций Modbus
1. Добавить обработку в `modbus_client.c`
2. Обновить парсинг в `device_list.c`
3. Добавить константы в `common.h`

### Добавление новых протоколов
1. Создать новый модуль (напр. `mqtt_client.c`)
2. Добавить инициализацию в `main.c`
3. Обновить конфигурацию
