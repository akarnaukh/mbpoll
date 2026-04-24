# API Документация MBusRead

## WebSocket API

### Подключение

```
ws://<host>:<websocket_port>/
```

**Пример:**
```bash
wscat -c ws://localhost:24123
```

### Формат сообщений

#### Исходящие сообщения (от сервера)

##### 1. Данные опроса устройств

Сервер отправляет данные после опроса каждого устройства или всех устройств (в зависимости от настройки `ws_request_output`).

**Структура:**
```json
{
  "devices": [
    {
      "address": <int>,
      "available": <boolean>,
      "registers": [
        {
          "function": <int>,
          "start": <int>,
          "quantity": <int>,
          "values": [<int>...] | ["na", ...]
        }
      ]
    }
  ],
  "timestamp": <unix_timestamp>
}
```

**Поля:**
- `devices` — массив данных устройств
  - `address` — адрес Modbus устройства (1-247)
  - `available` — статус доступности устройства
  - `registers` — массив диапазонов регистров
    - `function` — функция Modbus (2, 3, 4, etc.)
    - `start` — стартовый адрес регистра
    - `quantity` — количество регистров
    - `values` — значения регистров или "na" если недоступно
- `timestamp` — метка времени Unix (seconds)

**Пример успешного опроса:**
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
    }
  ],
  "timestamp": 1672156800
}
```

**Пример с недоступным устройством:**
```json
{
  "devices": [
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
  "timestamp": 1672156805
}
```

##### 2. Результат команды записи

Отправляется после выполнения команды записи в Modbus устройство.

**Структура:**
```json
{
  "type": "write_result",
  "success": <boolean>,
  "error": "<string or null>",
  "details": {
    "unitid": <int>,
    "function": <int>,
    "address": <int>,
    "quantity": <int>
  }
}
```

**Пример успеха:**
```json
{
  "type": "write_result",
  "success": true,
  "error": null,
  "details": {
    "unitid": 210,
    "function": 16,
    "address": 5,
    "quantity": 4
  }
}
```

**Пример ошибки:**
```json
{
  "type": "write_result",
  "success": false,
  "error": "Connection timeout",
  "details": {
    "unitid": 210,
    "function": 16,
    "address": 5,
    "quantity": 4
  }
}
```

---

#### Входящие сообщения (к серверу)

##### 1. Команда записи Modbus

Клиент может отправить команду на запись в устройство Modbus.

**Формат для записи нескольких регистров (FC 16):**
```json
{
  "value": [<int>, ...],
  "fc": 16,
  "unitid": <int>,
  "address": <int>,
  "quantity": <int>
}
```

**Пример:**
```json
{
  "value": [5000, 2300, 4000, 7300],
  "fc": 16,
  "unitid": 210,
  "address": 5,
  "quantity": 4
}
```

**Формат для записи одного регистра (FC 6):**
```json
{
  "value": <int>,
  "fc": 6,
  "unitid": <int>,
  "address": <int>,
  "quantity": 1
}
```

**Пример:**
```json
{
  "value": 25,
  "fc": 6,
  "unitid": 210,
  "address": 5,
  "quantity": 1
}
```

**Формат для записи одного дискретного выхода (FC 5):**
```json
{
  "value": <0 или 1>,
  "fc": 5,
  "unitid": <int>,
  "address": <int>,
  "quantity": 1
}
```

**Пример:**
```json
{
  "value": 1,
  "fc": 5,
  "unitid": 15,
  "address": 10,
  "quantity": 1
}
```

**Формат для записи нескольких дискретных выходов (FC 15):**
```json
{
  "value": [<0 или 1>, ...],
  "fc": 15,
  "unitid": <int>,
  "address": <int>,
  "quantity": <int>
}
```

**Пример:**
```json
{
  "value": [1, 0, 1, 1, 0],
  "fc": 15,
  "unitid": 15,
  "address": 10,
  "quantity": 5
}
```

**Поля команды записи:**
- `value` — значение(я) для записи (число или массив)
- `fc` — функция Modbus (5, 6, 15, 16)
- `unitid` — адрес устройства Modbus (1-247)
- `address` — адрес регистра/катушки (0-based)
- `quantity` — количество регистров/катушек

---

## Конфигурационный файл API

### Параметры связанные с API

```ini
# Порт WebSocket сервера
websocket_port = 24123

# IP адрес для прослушивания
listing_ip = 0.0.0.0

# Режим вывода WebSocket
# 1 - отправлять данные после опроса каждого устройства
# 0 - отправлять данные после опроса всех устройств
ws_request_output = 1
```

---

## Примеры использования

### Python клиент

```python
import websocket
import json
import time

def on_message(ws, message):
    data = json.loads(message)
    print(f"Received data from {len(data['devices'])} devices")
    for device in data['devices']:
        if device['available']:
            print(f"  Device {device['address']}: OK")
            for reg in device['registers']:
                print(f"    FC{reg['function']}: {reg['values']}")
        else:
            print(f"  Device {device['address']}: UNAVAILABLE")

def on_error(ws, error):
    print(f"Error: {error}")

def on_close(ws, close_status_code, close_msg):
    print("Connection closed")

def on_open(ws):
    print("Connected to MBusRead")
    
    # Отправка команды записи (опционально)
    write_command = {
        "value": [100, 200, 300],
        "fc": 16,
        "unitid": 10,
        "address": 0,
        "quantity": 3
    }
    ws.send(json.dumps(write_command))
    print("Sent write command")

if __name__ == "__main__":
    ws = websocket.WebSocketApp(
        "ws://localhost:24123/",
        on_open=on_open,
        on_message=on_message,
        on_error=on_error,
        on_close=on_close
    )
    
    ws.run_forever()
```

### Node.js клиент

```javascript
const WebSocket = require('ws');

const ws = new WebSocket('ws://localhost:24123/');

ws.on('open', function open() {
    console.log('Connected to MBusRead');
    
    // Отправка команды записи
    const writeCommand = {
        value: [5000, 2300],
        fc: 16,
        unitid: 210,
        address: 5,
        quantity: 2
    };
    
    ws.send(JSON.stringify(writeCommand));
    console.log('Sent write command');
});

ws.on('message', function incoming(data) {
    const parsed = JSON.parse(data);
    console.log('Received data:', JSON.stringify(parsed, null, 2));
    
    parsed.devices.forEach(device => {
        if (device.available) {
            console.log(`Device ${device.address}: OK`);
            device.registers.forEach(reg => {
                console.log(`  FC${reg.function}: ${reg.values.join(', ')}`);
            });
        } else {
            console.log(`Device ${device.address}: UNAVAILABLE`);
        }
    });
});

ws.on('error', function error(err) {
    console.error('WebSocket error:', err);
});

ws.on('close', function close() {
    console.log('Disconnected from MBusRead');
});
```

### Bash + wscat

```bash
#!/bin/bash

# Установка wscat
# npm install -g ws

# Подключение к WebSocket
wscat -c ws://localhost:24123

# После подключения можно отправить команду записи:
echo '{"value":[100,200],"fc":16,"unitid":10,"address":0,"quantity":2}' | wscat -c ws://localhost:24123

# Мониторинг данных (в реальном времени)
wscat -c ws://localhost:24123 | while read line; do
    echo "$line" | jq '.devices[] | select(.available==true) | {address, registers}'
done
```

### C++ клиент (using Boost.Beast)

```cpp
#include <boost/beast/websocket.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <iostream>
#include <json/json.h>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace asio = boost::asio;
using tcp = asio::ip::tcp;

class MBusReadClient {
    websocket::stream<tcp::socket> ws_;
    
public:
    MBusReadClient(asio::io_context& ioc)
        : ws_(ioc) {}
    
    void connect(const std::string& host, const std::string& port) {
        tcp::resolver resolver{ioc_};
        auto results = resolver.resolve(host, port);
        ws_.next_layer().connect(results);
        ws_.handshake(host, "/");
        std::cout << "Connected to MBusRead\n";
    }
    
    void sendWriteCommand(int unitid, int function, int address, 
                          int quantity, const std::vector<int>& values) {
        Json::Value command;
        command["unitid"] = unitid;
        command["fc"] = function;
        command["address"] = address;
        command["quantity"] = quantity;
        
        for (int v : values) {
            command["value"].append(v);
        }
        
        Json::StreamWriterBuilder builder;
        std::string jsonStr = Json::writeString(builder, command);
        ws_.write(asio::buffer(jsonStr));
    }
    
    void readLoop() {
        beast::flat_buffer buffer;
        while (true) {
            ws_.read(buffer);
            std::string jsonStr = beast::buffers_to_string(buffer.data());
            buffer.consume(buffer.size());
            
            // Parse and process JSON...
            std::cout << "Received: " << jsonStr << "\n";
        }
    }
};
```

---

## Коды ошибок Modbus

При выполнении команд записи могут возникнуть следующие ошибки:

| Код | Название | Описание |
|-----|----------|----------|
| 1 | Illegal Function | Неподдерживаемая функция |
| 2 | Illegal Data Address | Недопустимый адрес данных |
| 3 | Illegal Data Value | Недопустимое значение данных |
| 4 | Slave Device Failure | Ошибка подчиненного устройства |
| 5 | Acknowledge | Подтверждение (длительная операция) |
| 6 | Slave Device Busy | Устройство занято |
| 8 | Memory Parity Error | Ошибка четности памяти |
| 10 | Gateway Path Unavailable | Путь шлюза недоступен |
| 11 | Gateway Target Failed | Целевое устройство шлюза не ответило |

---

## Best Practices

### 1. Обработка отключений

```python
import websocket
import time

class RobustMBusReadClient:
    def __init__(self, url):
        self.url = url
        self.ws = None
        self.connected = False
        
    def connect(self):
        while not self.connected:
            try:
                self.ws = websocket.create_connection(self.url, timeout=5)
                self.connected = True
                print("Connected")
            except Exception as e:
                print(f"Connection failed: {e}, retrying in 5s...")
                time.sleep(5)
    
    def receive_with_reconnect(self):
        while True:
            try:
                data = self.ws.recv()
                return data
            except websocket.WebSocketConnectionClosedException:
                print("Connection closed, reconnecting...")
                self.connected = False
                self.connect()
            except Exception as e:
                print(f"Error: {e}")
                time.sleep(1)
```

### 2. Валидация данных

```javascript
function validateDeviceData(data) {
    if (!data.devices || !Array.isArray(data.devices)) {
        throw new Error('Invalid data format: missing devices array');
    }
    
    for (const device of data.devices) {
        if (typeof device.address !== 'number') {
            throw new Error('Invalid device: missing address');
        }
        
        if (typeof device.available !== 'boolean') {
            throw new Error('Invalid device: missing available flag');
        }
        
        if (device.available && (!device.registers || !Array.isArray(device.registers))) {
            throw new Error('Invalid device: missing registers');
        }
    }
    
    return true;
}
```

### 3. Rate Limiting

```python
import time
from collections import deque

class RateLimitedClient:
    def __init__(self, max_commands_per_second=10):
        self.max_commands = max_commands_per_second
        self.timestamps = deque()
    
    def send_command(self, ws, command):
        now = time.time()
        
        # Удаляем старые временные метки
        while self.timestamps and self.timestamps[0] < now - 1:
            self.timestamps.popleft()
        
        # Проверяем лимит
        if len(self.timestamps) >= self.max_commands:
            sleep_time = 1 - (now - self.timestamps[0])
            if sleep_time > 0:
                time.sleep(sleep_time)
        
        # Отправляем команду
        ws.send(json.dumps(command))
        self.timestamps.append(time.time())
```

---

## Тестирование API

### Тестовые сценарии

#### 1. Проверка подключения

```bash
timeout 5 wscat -c ws://localhost:24123 && echo "OK" || echo "FAILED"
```

#### 2. Проверка получения данных

```bash
timeout 10 wscat -c ws://localhost:24123 | head -n 1 | jq '.devices | length'
```

#### 3. Тест записи регистра

```bash
echo '{"value":[100],"fc":6,"unitid":10,"address":0,"quantity":1}' | \
  wscat -c ws://localhost:24123 -x
```

#### 4. Стресс-тест (множество подключений)

```bash
for i in {1..100}; do
  wscat -c ws://localhost:24123 &
done
wait
```

---

## Troubleshooting

### Проблема: Нет данных от устройств

**Решение:**
1. Проверьте логи сервиса:
   ```bash
   journalctl -u mbusread@coold1 -f
   ```
2. Убедитесь, что устройства доступны в сети
3. Проверьте конфигурацию `dev_list.json`

### Проблема: WebSocket не подключается

**Решение:**
1. Проверьте, что порт открыт:
   ```bash
   netstat -tlnp | grep 24123
   ```
2. Проверьте firewall:
   ```bash
   sudo ufw status
   sudo ufw allow 24123/tcp
   ```

### Проблема: Ошибка при записи

**Решение:**
1. Проверьте формат команды (правильность полей)
2. Убедитесь, что устройство поддерживает функцию записи
3. Проверьте права доступа к регистрам
