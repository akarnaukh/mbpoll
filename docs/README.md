# Документация MBusRead

Эта директория содержит подробную документацию по проекту MBusRead.

## 📚 Содержание

### Основная документация

| Документ | Описание |
|----------|----------|
| [ARCHITECTURE.md](./ARCHITECTURE.md) | Архитектура системы, модульная структура, поток данных |
| [API.md](./API.md) | Полная спецификация WebSocket API |
| [DEPLOYMENT.md](./DEPLOYMENT.md) | Руководство по установке, настройке и обслуживанию |
| [CONTRIBUTING.md](./CONTRIBUTING.md) | Как внести вклад в развитие проекта |

### Быстрый старт

1. **Новичкам** — начните с [README.md](../README.md) в корне проекта
2. **Установка** — следуйте [DEPLOYMENT.md](./DEPLOYMENT.md)
3. **Разработка** — прочитайте [ARCHITECTURE.md](./ARCHITECTURE.md) и [CONTRIBUTING.md](./CONTRIBUTING.md)
4. **Интеграция** — используйте [API.md](./API.md) для подключения клиентов

## 🔧 Для разработчиков

### Структура исходного кода

```
src/
├── main.c              # Точка входа, основной цикл
├── config.c/h          # Загрузка конфигурации
├── device_list.c/h     # Парсинг списка устройств JSON
├── modbus_client.c/h   # Modbus протокол
├── websocket.c/h       # WebSocket сервер
└── daemon.c/h          # Логирование и демонизация
```

### Сборка

```bash
# Стандартная сборка
make all

# Отладочная сборка
make debug

# Только компиляция
make build

# Очистка
make clean
```

### Тестирование

```bash
# Запуск тестовых скриптов
wscat -c ws://localhost:24123
./install_and_test.sh
./check_libmodbus.sh
```

## 📖 Для пользователей

### Конфигурация

Основные файлы конфигурации:

- `/etc/mbusread/*.conf` — конфигурация экземпляров сервиса
- `/etc/mbusread/dev_list.json` — список опрашиваемых устройств

### Управление сервисом

```bash
# Запуск
sudo systemctl start mbusread@coold1

# Статус
sudo systemctl status mbusread@coold1

# Логи
sudo journalctl -u mbusread@coold1 -f
```

### Подключение клиентов

```bash
# WebSocket подключение
wscat -c ws://localhost:24123
```

## 🔍 Поиск информации

### Я хочу...

- **Установить MBusRead** → [DEPLOYMENT.md](./DEPLOYMENT.md)
- **Настроить опрос устройств** → [DEPLOYMENT.md](./DEPLOYMENT.md#настройка-конфигурации)
- **Подключить клиент** → [API.md](./API.md)
- **Понять как работает** → [ARCHITECTURE.md](./ARCHITECTURE.md)
- **Исправить ошибку** → [CONTRIBUTING.md](./CONTRIBUTING.md)
- **Добавить функцию** → [CONTRIBUTING.md](./CONTRIBUTING.md)
- **Изучить код** → [ARCHITECTURE.md](./ARCHITECTURE.md#модульная-структура)

## ❓ FAQ

### Чем MBusRead отличается от mbpoll?

MBusRead — это демонизированный сервис с WebSocket API, работающий постоянно, 
в то время как mbpoll — утилита командной строки для разового опроса.

### Какие устройства поддерживаются?

- Modbus TCP/IP устройства
- Modbus RTU через последовательный порт (RS-485/RS-232)
- Любые устройства, поддерживающие функции 2, 3, 4, 5, 6, 15, 16

### Сколько устройств можно опрашивать?

Зависит от:
- Интервала опроса каждого устройства
- Общей нагрузки на систему
- Сетевых задержек

Рекомендуется не более 50-100 устройств на экземпляр сервиса.

### Как масштабировать?

Запустите несколько экземпляров сервиса:

```bash
sudo systemctl start mbusread@substation1
sudo systemctl start mbusread@substation2
sudo systemctl start mbusread@substation3
```

Каждый экземпляр имеет свою конфигурацию и опрашивает свою группу устройств.

## 🔗 Ресурсы

- **GitHub репозиторий**: https://github.com/akarnaukh/mbpoll
- **Issues**: https://github.com/akarnaukh/mbpoll/issues
- **Modbus спецификация**: https://modbus.org/specs.php
- **libmodbus**: https://libmodbus.org/documentation/

## 📞 Поддержка

- **Баги и предложения**: Создайте issue на GitHub
- **Вопросы**: Используйте discussions или создайте issue с меткой "question"
- **Консультации**: Свяжитесь через GitHub issues

## 📝 Лицензия

Проект распространяется под лицензией MIT. См. [LICENSE](../LICENSE) для деталей.

---

**Последнее обновление**: Апрель 2024

**Версия документации**: 0.1
