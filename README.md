# MBusRead - ModBus TCP/RTU Polling Service

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![Platform](https://img.shields.io/badge/platform-Linux-green.svg)
![Protocol](https://img.shields.io/badge/protocol-Modbus%20TCP%2FRTU-orange.svg)

## 📖 Описание
MBusRead - это система опроса Modbus устройств, работающая как демон systemd. Сервис опрашивает устройства в соответствии с конфигурационным файлом и указанным интервалом, предоставляя данные через WebSocket. 

## ✨ Основные возможности:
- **Поддержка протоколов**: Modbus TCP и RTU (последовательный порт)
- **Функции Modbus**: 2 (Input Bits), 3 (Holding Registers), 4 (Input Registers), 5, 15, 6, 16
- **Управление устройствами**: Гибкая конфигурация через JSON файл
- **Отказоустойчивость**: Автоматическое переподключение при потере соединения
- **Мониторинг**: Подробное логирование с различными уровнями
- **Доступ к данным**: WebSocket с JSON форматом (как после опроса каждого устройства, так и после полного опроса)
- **Масштабируемость**: Поддержка множества экземпляров через systemd templates
- **Запись данных**: При получении запроса на запись, опрос приостанавливается, производится запрос на запись, опрос из списка продолжается

## 🚀 Быстрый старт

### 1. Установка зависимостей
```bash
sudo apt update
sudo apt install -y libmodbus-dev libjansson-dev build-essential libssl-dev
```

### 2. Сборка и установка
```bash
git clone https://github.com/akarnaukh/mbpoll.git
cd mbusread
make all
sudo make install
```

### 3. Настройка
```bash
sudo nano /etc/mbusread/coold1.conf
sudo nano /etc/mbusread/dev_list.json
```

### 4. Запуск сервиса
```bash
sudo systemctl enable mbusread@coold1
sudo systemctl start mbusread@coold1
sudo systemctl status mbusread@coold1
```

### 5. Подключение к WebSocket
```bash
wscat -c ws://localhost:24123
```

## 📚 Документация

| Документ | Описание |
|----------|----------|
| [🏗 Архитектура](docs/ARCHITECTURE.md) | Подробное описание архитектуры, модулей и потока данных |
| [🔌 API](docs/API.md) | Спецификация WebSocket API, форматы сообщений, примеры кода |
| [📦 Развертывание](docs/DEPLOYMENT.md) | Руководство по установке, настройке и обслуживанию |
| [🤝 Вклад в проект](docs/CONTRIBUTING.md) | Как сообщать об ошибках, предлагать улучшения и писать код |
| [📖 Навигатор](docs/README.md) | Обзор всей документации и FAQ |

## ⚙️ Конфигурация

### Структура файлов
```text
/etc/mbusread/
├── coold1.conf              # Конфигурация экземпляра
└── dev_list.json           # Список устройств
```

### Пример конфигурации `/etc/mbusread/coold1.conf`:
```ini
# ModBus Read Service Configuration
# Serial port settings 
# device = /dev/ttyS1@9600 
# or Modbus TCP device settings
device = 192.168.0.10:502

# Интервал между опросом в мс 
poll_interval_ms = 5000 

# IP адрес для прослушивания WebSocket
listing_ip = 0.0.0.0

# Порт WebSocket 
websocket_port = 24123

# Отправка данных после опроса каждого устройства 1 или в конце опроса всех 0 из списка
ws_request_output = 1

log_level = info
# Доступы уровни - debug: Все пишем в лог, включая данные регистров,
# info: События modbus, warn - timeout при чтении,
# error - только критические ошибки

# Файл списка устройств
dev_list_file = /etc/mbusread/dev_list.json
```

### Пример файла списка устройств `/etc/mbusread/dev_list.json`:
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
    "11-15": {
      "3": {
        "s": 0,
        "q": 10
      }
    }
  }
}
```

#### Структура JSON:
- **devices** - объект с устройствами
  - **Ключ**: адрес устройства (например, `"10"` или диапазон `"12-15"`)
  - **Значение**: объект с функциями Modbus
    - **Ключ**: номер функции (2, 3, 4)
    - **Значение**: объект с параметрами
      - **s**: стартовый адрес регистра
      - **q**: количество регистров

#### Формат запроса на запись:
```json
// Запись нескольких регистров (FC 16)
{
    "value": [5000, 2300, 4000, 7300],
    "fc": 16,
    "unitid": 210,
    "address": 5,
    "quantity": 4 
}

// Запись одного регистра (FC 6)
{
    "value": 25,
    "fc": 6,
    "unitid": 210,
    "address": 5,
    "quantity": 4
}
```

#### Поддерживаемые функции Modbus:
- `2` - Input Bits (дискретные входы)
- `3` - Holding Registers (регистры хранения)
- `4` - Input Registers (входные регистры)
- `5`, `6`, `15`, `16` - Функции записи

---

## 🛠 Установка и сборка

### Предварительные требования
```bash
sudo apt update
sudo apt install -y libmodbus-dev libjansson-dev build-essential libssl-dev
```

### Сборка из исходников
```bash
git clone https://github.com/akarnaukh/mbpoll.git
cd mbusread
make all
sudo make install
```

### Makefile цели
```bash
make all           # Очистка, сборка и создание шаблонов
make clean         # Очистка сборки
make debug         # Сборка с отладочной информацией
make install       # Установка в систему
make uninstall     # Удаление из системы
make template      # Создание шаблонов конфигурации
make monitor       # Мониторинг логов сервиса
make ws-test       # Тестирование WebSocket клиента
```


## 🔧 Управление сервисом

### Запуск и остановка
```bash
# Запуск одного экземпляра
sudo systemctl start mbusread@coold1

# Автозапуск при загрузке
sudo systemctl enable mbusread@coold1

# Проверка статуса
sudo systemctl status mbusread@coold1

# Остановка сервиса
sudo systemctl stop mbusread@coold1

# Перезапуск сервиса
sudo systemctl restart mbusread@coold1
```

### Просмотр логов
```bash
# Логи в реальном времени
sudo journalctl -u mbusread@coold1 -f

# Последние 50 строк логов
sudo journalctl -u mbusread@coold1 -n 50 --no-pager

# Логи с временными метками
sudo journalctl -u mbusread@coold1 -o short-precise
```

### Запуск нескольких экземпляров
```bash
# Создание конфигураций для разных устройств
sudo cp /etc/mbusread/coold1.conf /etc/mbusread/device2.conf
sudo nano /etc/mbusread/device2.conf

# Запуск нескольких экземпляров
sudo systemctl start mbusread@coold1
sudo systemctl start mbusread@device2

# Мониторинг всех экземпляров
sudo journalctl -u 'mbusread@*' -f
```

### Ручной запуск для отладки
```bash
# Запуск с выводом в консоль
/usr/local/bin/mbusread /etc/mbusread/coold1.conf

# Запуск с отладочным уровнем логов
sudo systemctl stop mbusread@coold1
sudo /usr/local/bin/mbusread /etc/mbusread/coold1.conf 2>&1 | tee /tmp/mbusread.log
```

---

## 📊 Формат данных WebSocket

### Формат ответа
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
        },
        {
          "function": 4,
          "start": 22,
          "quantity": 3,
          "values": [255, 2437, 1]
        }
      ]
    },
    {
      "address": 11,
      "available": false,
      "registers": [
        {
          "function": 3,
          "start": 1,
          "quantity": 5,
          "values": ["na", "na", "na", "na", "na"]
        }
      ]
    }
  ],
  "timestamp": 1672156800
}
```

### Поля ответа:
- **devices** - массив устройств
  - **address** - адрес Modbus устройства
  - **available** - доступность устройства (true/false)
  - **registers** - массив диапазонов регистров
    - **function** - функция Modbus
    - **start** - начальный адрес
    - **quantity** - количество регистров
    - **values** - значения регистров или `"na"` если недоступно
- **timestamp** - метка времени Unix

### Формат запроса на запись
```json
// Запись нескольких регистров (FC 16)
{
    "value": [5000, 2300, 4000, 7300],
    "fc": 16,
    "unitid": 210,
    "address": 5,
    "quantity": 4 
}

// Запись одного регистра (FC 6)
{
    "value": 25,
    "fc": 6,
    "unitid": 210,
    "address": 5,
    "quantity": 4
}
```

> ⚠️ **Примечание**: При получении запроса на запись, опрос приостанавливается, производится запрос на запись, затем опрос продолжается.

---

## 📋 Логирование

### Уровни логирования
| Уровень | Описание |
|---------|----------|
| `debug` | Все сообщения, включая значения регистров |
| `info` | Основные события (подключения, опросы) |
| `warn` | Предупреждения (таймауты, частичные чтения) |
| `error` | Критические ошибки |

### Пример логов
```text
INFO: === Starting mbusread service ===
INFO: Configuration file: /etc/mbusread/coold1.conf
INFO: === Step 1: Loading configuration ===
INFO: Device: 192.168.1.100:502
INFO: Modbus TCP device: 192.168.1.100:502
INFO: Poll interval: 5000 ms
INFO: WebSocket server will listen on: 0.0.0.0:24123
INFO: Configuration loaded successfully
INFO: === Step 2: Loading device list ===
INFO: Successfully loaded 2 devices
INFO: === Step 3: Initializing Modbus connection ===
INFO: Modbus TCP connection established: 192.168.1.100:502
INFO: === Step 5: Starting WebSocket server ===
INFO: WebSocket server listening on 0.0.0.0:24123
INFO: === Service initialization complete ===
DEBUG: === Polling cycle 1 ===
DEBUG: Polling device 10
DEBUG: Device 10 (func 3): 0015 0003 0003 ...
DEBUG: Device 10 polled successfully
```

---

## 🛡 Отказоустойчивость

### Обработка ошибок
| Ошибка | Действие |
|--------|----------|
| `Connection timed out` | Устройство помечается как недоступное |
| `Modbus устройство недоступно` | Последующие функции для устройства пропускаются |
| `Modbus соединение недоступно` | Попытка переинициализации соединения |
| `Некорректный JSON dev_list` | Критическая ошибка, сервис завершает работу |

### Переподключение при потере соединения
1. Логируется предупреждение
2. Закрывается существующее соединение
3. Ожидается 2 секунды
4. Выполняется попытка переподключения
5. При неудаче - экспоненциальная задержка до 30 секунд

### Управление памятью
1. **Выделение при старте**: Память для всех устройств и диапазонов регистров
2. **Динамическое выделение**: Значения регистров при первом успешном чтении
3. **Освобождение**: При недоступности устройства
4. **Очистка**: Все ресурсы при завершении работы

---

## 📁 Структура проекта

```text
mbusread/
├── src/                        # Исходный код
│   ├── main.c                  # Главная функция
│   ├── daemon.c                # Функции демонизации и логирования
│   ├── daemon.h                # Заголовочный файл
│   ├── config.c                # Загрузка конфигурации
│   ├── config.h                # Заголовочный файл
│   ├── modbus_client.c         # Modbus клиент
│   ├── modbus_client.h         # Заголовочный файл
│   ├── device_list.c           # Парсинг списка устройств
│   ├── device_list.h           # Заголовочный файл
│   └── websocket.c             # WebSocket сервер
├── build/                      # Собранные файлы
│   ├── mbusread                # Исполняемый файл
│   ├── systemd/
│   │   └── mbusread@.service   # Systemd unit файл
│   └── config/
│       └── mbusread.conf       # Шаблон конфигурации
├── Makefile                    # Файл сборки
├── README.md                   # Основная документация
└── docs/                       # Подробная документация
    ├── README.md               # Навигатор по документации
    ├── ARCHITECTURE.md         # Архитектура системы
    ├── API.md                  # Спецификация API
    ├── DEPLOYMENT.md           # Руководство по развертыванию
    └── CONTRIBUTING.md         # Руководство для контрибьюторов
```

---

## 💻 Разработка

### Сборка для разработки
```bash
# Сборка с отладочной информацией
make debug

# Проверка зависимостей
./check_libmodbus.sh
```

### Тестирование
```bash
# Тестирование WebSocket клиента
wscat -c ws://localhost:24123

# Тестирование полного цикла
./install_and_test.sh
```

### Отладка
```bash
# Запуск под strace
strace -f -o /tmp/mbusread.strace /usr/local/bin/mbusread /etc/mbusread/coold1.conf

# Запуск под gdb
gdb /usr/local/bin/mbusread
(gdb) run /etc/mbusread/coold1.conf

# Проверка утечек памяти
valgrind --leak-check=full /usr/local/bin/mbusread /etc/mbusread/coold1.conf
```

---

## 🔧 Устранение неполадок

### Распространенные проблемы

#### 1. Сервис не запускается
```bash
# Проверка конфигурационного файла
sudo /usr/local/bin/mbusread /etc/mbusread/coold1.conf

# Проверка логов systemd
sudo journalctl -u mbusread@coold1 -n 50 --no-pager

# Проверка прав доступа
ls -la /etc/mbusread/
sudo chmod 644 /etc/mbusread/coold1.conf
sudo chmod 644 /etc/mbusread/dev_list.json
```

#### 2. Modbus соединение постоянно теряется
```bash
# Увеличить интервал опроса в /etc/mbusread/coold1.conf:
poll_interval_ms = 10000
```

#### 3. WebSocket сервер не принимает соединения
```bash
# Проверка порта
sudo netstat -tlnp | grep 24123

# Проверка фаервола
sudo ufw status
sudo ufw allow 24123/tcp

# Тестирование подключения
wscat -c ws://localhost:24123
```

#### 4. Ошибка JSON парсинга
```bash
# Проверка синтаксиса JSON
python3 -m json.tool /etc/mbusread/dev_list.json

# Проверка структуры (должен быть объект "devices")
```

### Мониторинг производительности
```bash
# Использование памяти
ps aux | grep mbusread

# Количество WebSocket соединений
ss -t | grep 24123

# Частота опросов (лог каждые 100 циклов)
sudo journalctl -u mbusread@coold1 | grep "Statistics"
```

---

## 👥 Авторы

- [@AKA_ZejroN](https://github.com/akarnaukh)

---

## 📝 Лицензия

Этот проект распространяется под лицензией MIT. См. файл [LICENSE](LICENSE) для деталей.

---

**MBusRead** - ModBus TCP/RTU Polling Service © 2024
