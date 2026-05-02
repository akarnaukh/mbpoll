#include "websocket.h"
#include <sys/epoll.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

// WebSocket handshake ключевые слова
static const char WS_GUID[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

// Base64 кодирование с использованием OpenSSL
static char* base64_encode(const unsigned char *input, int length) {
    BIO *bmem, *b64;
    BUF_MEM *bptr;
    
    b64 = BIO_new(BIO_f_base64());
    bmem = BIO_new(BIO_s_mem());
    b64 = BIO_push(b64, bmem);
    BIO_write(b64, input, length);
    BIO_flush(b64);
    BIO_get_mem_ptr(b64, &bptr);
    
    char *buff = (char *)malloc(bptr->length + 1);
    memcpy(buff, bptr->data, bptr->length - 1);
    buff[bptr->length - 1] = '\0';
    
    BIO_free_all(b64);
    return buff;
}

// Проверка и обработка WebSocket handshake
static int handle_websocket_handshake(int client_fd) {
    char buffer[4096];
    ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    
    if (bytes_read <= 0) {
        return -1;
    }
    
    buffer[bytes_read] = '\0';
    
    // Проверяем, что это HTTP GET запрос
    if (strncmp(buffer, "GET ", 4) != 0) {
        return -1;
    }
    
    // Ищем WebSocket ключ
    char *key_start = strstr(buffer, "Sec-WebSocket-Key: ");
    if (!key_start) {
        // Попробуем другой формат
        key_start = strstr(buffer, "Sec-WebSocket-Key:");
        if (key_start) {
            key_start += strlen("Sec-WebSocket-Key:");
        } else {
            return -1;
        }
    } else {
        key_start += strlen("Sec-WebSocket-Key: ");
    }
    
    char *key_end = strstr(key_start, "\r\n");
    if (!key_end) {
        return -1;
    }
    
    // Извлекаем ключ
    char ws_key[256];
    unsigned int key_len = key_end - key_start;
    if (key_len >= sizeof(ws_key)) {
        return -1;
    }
    
    strncpy(ws_key, key_start, key_len);
    ws_key[key_len] = '\0';
    
    // Убираем пробелы в начале и конце
    while (key_len > 0 && (ws_key[0] == ' ' || ws_key[0] == '\t')) {
        memmove(ws_key, ws_key + 1, key_len);
        key_len--;
        ws_key[key_len] = '\0';
    }
    
    // Вычисляем accept ключ
    char combined_key[512];
    snprintf(combined_key, sizeof(combined_key), "%s%s", ws_key, WS_GUID);
    
    unsigned char sha1_hash[SHA_DIGEST_LENGTH];
    SHA1((unsigned char*)combined_key, strlen(combined_key), sha1_hash);
    
    char *accept_key = base64_encode(sha1_hash, SHA_DIGEST_LENGTH);
    if (!accept_key) {
        return -1;
    }
    
    // Формируем ответ
    char response[1024];
    snprintf(response, sizeof(response),
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: %s\r\n"
        "\r\n",
        accept_key);
    
    free(accept_key);
    
    // Отправляем ответ
    if (send(client_fd, response, strlen(response), 0) != (ssize_t)strlen(response)) {
        return -1;
    }
    
    return 0;
}

// Отправка WebSocket фрейма
static int send_websocket_frame(int fd, int opcode, const char *data, size_t len) {
    unsigned char header[14];
    size_t header_size = 2;
    
    header[0] = 0x80 | (opcode & 0x0F); // FIN бит + opcode
    
    if (len <= 125) {
        header[1] = len & 0x7F;
    } else if (len <= 65535) {
        header[1] = 126;
        header[2] = (len >> 8) & 0xFF;
        header[3] = len & 0xFF;
        header_size = 4;
    } else {
        header[1] = 127;
        // 64-bit length (старшие 32 бита нули)
        header[2] = 0; header[3] = 0; header[4] = 0; header[5] = 0;
        header[6] = (len >> 24) & 0xFF;
        header[7] = (len >> 16) & 0xFF;
        header[8] = (len >> 8) & 0xFF;
        header[9] = len & 0xFF;
        header_size = 10;
    }
    
    // Отправляем заголовок
    if (send(fd, header, header_size, 0) != (ssize_t)header_size) {
        return -1;
    }
    
    // Отправляем данные, если есть
    if (len > 0 && data != NULL) {
        if (send(fd, data, len, 0) != (ssize_t)len) {
            return -1;
        }
    }
    
    return 0;
}

// Чтение WebSocket фрейма
static ssize_t read_websocket_frame(int fd, char *buffer, size_t buffer_size) {
    unsigned char header[2];
    ssize_t bytes_read = recv(fd, header, 2, 0);
    if (bytes_read != 2) {
        return -1;
    }
    
//    int fin = (header[0] & 0x80) != 0;
    int opcode = header[0] & 0x0F;
    int masked = (header[1] & 0x80) != 0;
    uint64_t payload_len = header[1] & 0x7F;
    
    // Читаем расширенную длину
    if (payload_len == 126) {
        unsigned char len_bytes[2];
        if (recv(fd, len_bytes, 2, 0) != 2) return -1;
        payload_len = (len_bytes[0] << 8) | len_bytes[1];
    } else if (payload_len == 127) {
        unsigned char len_bytes[8];
        if (recv(fd, len_bytes, 8, 0) != 8) return -1;
        payload_len = 0;
        for (int i = 0; i < 8; i++) {
            payload_len = (payload_len << 8) | len_bytes[i];
        }
    }
    
    // Проверяем размер буфера
    if (payload_len > buffer_size - 1) {
        return -1;
    }
    
    // Читаем маску если есть
    unsigned char mask[4];
    if (masked) {
        if (recv(fd, mask, 4, 0) != 4) return -1;
    }
    
    // Читаем данные
    bytes_read = recv(fd, buffer, payload_len, 0);
    if (bytes_read != (ssize_t)payload_len) {
        return -1;
    }
    
    // Применяем маску если есть
    if (masked) {
        for (size_t i = 0; i < payload_len; i++) {
            buffer[i] ^= mask[i % 4];
        }
    }
    
    buffer[payload_len] = '\0';
    
    // Обрабатываем специальные фреймы
    if (opcode == 0x8) { // CLOSE
        return -2;
    } else if (opcode == 0x9) { // PING
        // Отвечаем PONG
        send_websocket_frame(fd, 0xA, buffer, payload_len);
        return 0;
    } else if (opcode == 0xA) { // PONG
        return 0; // Игнорируем
    }
    
    return payload_len;
}

// Инициализация WebSocket сервера
int init_websocket_server(config_t *config) {
    log_info("Initializing WebSocket server on port %d", config->websocket_port);
    
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        log_error("WebSocket: socket creation failed: %s", strerror(errno));
        return -1;
    }
    
    // Позволяем повторно использовать адрес и порт
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        log_error("WebSocket: setsockopt SO_REUSEADDR failed: %s", strerror(errno));
        close(server_fd);
        return -1;
    }
    
#ifdef SO_REUSEPORT
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt))) {
        log_error("WebSocket: setsockopt SO_REUSEPORT failed: %s", strerror(errno));
        close(server_fd);
        return -1;
    }
#endif
    
    // Устанавливаем неблокирующий режим
    int flags = fcntl(server_fd, F_GETFL, 0);
    fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);
    
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(config->listing_ip);
    address.sin_port = htons(config->websocket_port);
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        log_error("WebSocket: bind failed: %s", strerror(errno));
        close(server_fd);
        return -1;
    }
    
    if (listen(server_fd, 10) < 0) {
        log_error("WebSocket: listen failed: %s", strerror(errno));
        close(server_fd);
        return -1;
    }
    
    config->ws_server_fd = server_fd;
    config->ws_clients = NULL;
    pthread_mutex_init(&config->ws_mutex, NULL);
    
    log_info("WebSocket server initialized on %s:%d", 
             config->listing_ip, config->websocket_port);
    
    return 0;
}

// Поток WebSocket сервера
void *websocket_server_thread(void *arg) {
    config_t *config = (config_t *)arg;
    
    log_info("WebSocket server thread starting");
    
    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        log_error("WebSocket: epoll_create failed: %s", strerror(errno));
        return NULL;
    }
    
    // Добавляем серверный сокет в epoll
    struct epoll_event event;
    event.events = EPOLLIN | EPOLLET; // Edge-triggered
    event.data.fd = config->ws_server_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, config->ws_server_fd, &event) < 0) {
        log_error("WebSocket: epoll_ctl server failed: %s", strerror(errno));
        close(epoll_fd);
        return NULL;
    }
    
    struct epoll_event events[100];
    
    while (g_running) {
        int nfds = epoll_wait(epoll_fd, events, 100, 100); // Таймаут 100ms
        
        if (nfds < 0 && errno != EINTR) {
            log_error("WebSocket: epoll_wait failed: %s", strerror(errno));
            break;
        }
        
        if (!g_running) {
            break;
        }
        
        for (int i = 0; i < nfds; i++) {
            // Новое соединение
            if (events[i].data.fd == config->ws_server_fd) {
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int client_fd = accept(config->ws_server_fd, 
                                      (struct sockaddr *)&client_addr, 
                                      &client_len);
                
                if (client_fd < 0) {
                    if (errno != EWOULDBLOCK && errno != EAGAIN) {
                        log_error("WebSocket: accept failed: %s", strerror(errno));
                    }
                    continue;
                }
                
                // Устанавливаем неблокирующий режим
                int flags = fcntl(client_fd, F_GETFL, 0);
                fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
                
                // Обрабатываем handshake
                if (handle_websocket_handshake(client_fd) == 0) {
                    // Добавляем в epoll
                    event.events = EPOLLIN | EPOLLET;
                    event.data.fd = client_fd;
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &event) < 0) {
                        log_error("WebSocket: epoll_ctl client failed: %s", strerror(errno));
                        close(client_fd);
                        continue;
                    }
                    
                    // Добавляем клиента в список
                    pthread_mutex_lock(&config->ws_mutex);
                    websocket_client_t *new_client = malloc(sizeof(websocket_client_t));
                    if (new_client) {
                        new_client->fd = client_fd;
                        new_client->last_active = time(NULL);
                        new_client->next = config->ws_clients;
                        config->ws_clients = new_client;
                        
                        char client_ip[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
                        log_info("WebSocket client connected from %s:%d", 
                                client_ip, ntohs(client_addr.sin_port));
                    }
                    pthread_mutex_unlock(&config->ws_mutex);
                } else {
                    log_error("WebSocket handshake failed");
                    close(client_fd);
                }
            } else {
                // Данные от клиента
                int client_fd = events[i].data.fd;
                char buffer[4096];
                ssize_t result = read_websocket_frame(client_fd, buffer, sizeof(buffer));
                
                if (result == -2) {
                    // Закрытие соединения
                    log_debug("WebSocket client closed connection");
                    pthread_mutex_lock(&config->ws_mutex);
                    websocket_client_t **prev = &config->ws_clients;
                    websocket_client_t *current = config->ws_clients;
                    
                    while (current) {
                        if (current->fd == client_fd) {
                            *prev = current->next;
                            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                            close(client_fd);
                            free(current);
                            break;
                        }
                        prev = &current->next;
                        current = current->next;
                    }
                    pthread_mutex_unlock(&config->ws_mutex);
                } else if (result <= 0) {
                    // Ошибка или закрытие
                    if (result < 0) {
                        log_debug("WebSocket client disconnected (error)");
                    }
                    pthread_mutex_lock(&config->ws_mutex);
                    websocket_client_t **prev = &config->ws_clients;
                    websocket_client_t *current = config->ws_clients;
                    
                    while (current) {
                        if (current->fd == client_fd) {
                            *prev = current->next;
                            epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                            close(client_fd);
                            free(current);
                            break;
                        }
                        prev = &current->next;
                        current = current->next;
                    }
                    pthread_mutex_unlock(&config->ws_mutex);
                } else {
                    // Обновляем время активности
                    pthread_mutex_lock(&config->ws_mutex);
                    websocket_client_t *client = config->ws_clients;
                    while (client) {
                        if (client->fd == client_fd) {
                            client->last_active = time(NULL);
                            break;
                        }
                        client = client->next;
                    }
                    pthread_mutex_unlock(&config->ws_mutex);
                    
                    // Логируем полученные данные
                    if (result > 0 && config->log_level >= LOG_LEVEL_DEBUG) {
                        buffer[result < 100 ? result : 100] = '\0';
                        log_debug("WebSocket received: %s", buffer);
                    }
                    
                    // Обрабатываем команду записи Modbus если это JSON с командой
                    if (result > 0) {
                        process_modbus_write_command(config, buffer);
                    }
                }
            }
        }
        
        // Очищаем неактивных клиентов (таймаут 1 минута)
        time_t now = time(NULL);
        pthread_mutex_lock(&config->ws_mutex);
        websocket_client_t **prev = &config->ws_clients;
        websocket_client_t *current = config->ws_clients;
        
        while (current) {
            if (now - current->last_active > 60) { // 1 минута
                log_debug("Closing inactive WebSocket client");
                *prev = current->next;
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, current->fd, NULL);
                close(current->fd);
                free(current);
                current = *prev;
            } else {
                prev = &current->next;
                current = current->next;
            }
        }
        pthread_mutex_unlock(&config->ws_mutex);
    }
    
    // Закрываем все соединения
    pthread_mutex_lock(&config->ws_mutex);
    websocket_client_t *client = config->ws_clients;
    while (client) {
        websocket_client_t *next = client->next;
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client->fd, NULL);
        close(client->fd);
        free(client);
        client = next;
    }
    config->ws_clients = NULL;
    pthread_mutex_unlock(&config->ws_mutex);
    
    close(epoll_fd);
    close(config->ws_server_fd);
    
    log_info("WebSocket server thread stopped");
    return NULL;
}

// Вещание данных всем WebSocket клиентам
void broadcast_to_websockets(config_t *config, const char *data, size_t len) {
    if (len == 0 || data == NULL) return;
    
    pthread_mutex_lock(&config->ws_mutex);
    
    if (config->ws_clients == NULL) {
        pthread_mutex_unlock(&config->ws_mutex);
        return;
    }
    
    websocket_client_t *client = config->ws_clients;
    websocket_client_t *prev = NULL;
    int active_clients = 0;
    
    while (client) {
        // Отправляем данные
        if (send_websocket_frame(client->fd, 0x1, data, len) < 0) { // TEXT frame
            // Ошибка отправки, закрываем соединение
            log_debug("WebSocket send failed, closing connection (fd: %d)", client->fd);
            
            // Удаляем из списка
            if (prev) {
                prev->next = client->next;
            } else {
                config->ws_clients = client->next;
            }
            
            websocket_client_t *to_free = client;
            client = client->next;
            close(to_free->fd);
            free(to_free);
            continue;
        }
        
        client->last_active = time(NULL);
        active_clients++;
        prev = client;
        client = client->next;
    }
    
    pthread_mutex_unlock(&config->ws_mutex);
    
    if (config->log_level >= LOG_LEVEL_DEBUG && active_clients > 0) {
        log_debug("Broadcasted data to %d WebSocket clients (%zu bytes)", 
                 active_clients, len);
    }
}

// Закрытие WebSocket клиента
void close_websocket_client(websocket_client_t *client) {
    if (client) {
        close(client->fd);
        free(client);
    }
}


// Отправка данных отдельного запроса всем WebSocket клиентам
void send_request_to_websockets(config_t *config, int device_address, int function, 
                                 int start, int quantity, int success, uint16_t *values) {
    if (!config->ws_request_output || config->websocket_port <= 0) {
        return; // Функция отключена или WebSocket не запущен
    }
    
    pthread_mutex_lock(&config->ws_mutex);
    
    if (config->ws_clients == NULL) {
        pthread_mutex_unlock(&config->ws_mutex);
        return; // Нет клиентов
    }
    
    // Формируем JSON в том же формате как и полный результат опроса
    json_t *root = json_object();
    json_t *devices_array = json_array();
    
    json_t *device_obj = json_object();
    json_object_set_new(device_obj, "address", json_integer(device_address));
    json_object_set_new(device_obj, "available", json_boolean(success));
    
    json_t *registers_array = json_array();
    json_t *range_obj = json_object();
    json_object_set_new(range_obj, "function", json_integer(function));
    json_object_set_new(range_obj, "start", json_integer(start));
    json_object_set_new(range_obj, "quantity", json_integer(quantity));
    
    json_t *values_array = json_array();
    if (success && values) {
        for (int i = 0; i < quantity; i++) {
            json_array_append_new(values_array, json_integer(values[i]));
        }
    } else {
        for (int i = 0; i < quantity; i++) {
            json_array_append_new(values_array, json_string("na"));
        }
    }
    
    json_object_set_new(range_obj, "values", values_array);
    json_array_append_new(registers_array, range_obj);
    json_object_set_new(device_obj, "registers", registers_array);
    json_array_append_new(devices_array, device_obj);
    
    json_object_set_new(root, "devices", devices_array);
    json_object_set_new(root, "timestamp", json_integer(time(NULL)));
    json_object_set_new(root, "single_request", json_boolean(1));
    
    char *json_str = json_dumps(root, JSON_COMPACT);
    json_decref(root);
    
    if (!json_str) {
        pthread_mutex_unlock(&config->ws_mutex);
        log_error("Failed to create JSON for request output");
        return;
    }
    
    // Отправляем всем клиентам
    websocket_client_t *client = config->ws_clients;
    websocket_client_t *prev = NULL;
    int active_clients = 0;
    
    while (client) {
        if (send_websocket_frame(client->fd, 0x1, json_str, strlen(json_str)) < 0) {
            // Ошибка отправки, закрываем соединение
            log_debug("WebSocket send failed, closing connection (fd: %d)", client->fd);
            
            if (prev) {
                prev->next = client->next;
            } else {
                config->ws_clients = client->next;
            }
            
            websocket_client_t *to_free = client;
            client = client->next;
            close(to_free->fd);
            free(to_free);
            continue;
        }
        
        client->last_active = time(NULL);
        active_clients++;
        prev = client;
        client = client->next;
    }
    
    pthread_mutex_unlock(&config->ws_mutex);
    
    if (config->log_level >= LOG_LEVEL_DEBUG && active_clients > 0) {
        log_debug("Sent request data to %d WebSocket clients (%zu bytes)",
                 active_clients, strlen(json_str));
    }
    
    free(json_str);
}

// Отправка данных устройства всем WebSocket клиентам (для ws_request_output)
void send_device_to_websockets(config_t *config, modbus_device_t *device) {
    if (!config->ws_request_output || config->websocket_port <= 0) {
        return; // Функция отключена или WebSocket не запущен
    }
    
    pthread_mutex_lock(&config->ws_mutex);
    
    if (config->ws_clients == NULL) {
        pthread_mutex_unlock(&config->ws_mutex);
        return; // Нет клиентов
    }
    
    // Формируем JSON в том же формате как и полный результат опроса
    json_t *root = json_object();
    json_t *devices_array = json_array();
    
    json_t *device_obj = json_object();
    json_object_set_new(device_obj, "address", json_integer(device->slave_address));
    if (device->name) {
        json_object_set_new(device_obj, "name", json_string(device->name));
    }
    json_object_set_new(device_obj, "available", json_boolean(device->device_available));
    
    json_t *registers_array = json_array();
    
    for (int j = 0; j < device->range_count; j++) {
        register_range_t *range = &device->ranges[j];
        
        json_t *range_obj = json_object();
        json_object_set_new(range_obj, "function", json_integer(range->function));
        json_object_set_new(range_obj, "start", json_integer(range->start));
        json_object_set_new(range_obj, "quantity", json_integer(range->quantity));
        
        json_t *values_array = json_array();
        if (range->values && device->device_available) {
            for (int k = 0; k < range->quantity; k++) {
                json_array_append_new(values_array, json_integer(range->values[k]));
            }
        } else {
            for (int k = 0; k < range->quantity; k++) {
                json_array_append_new(values_array, json_string("na"));
            }
        }
        
        json_object_set_new(range_obj, "values", values_array);
        json_array_append_new(registers_array, range_obj);
    }
    
    json_object_set_new(device_obj, "registers", registers_array);
    json_array_append_new(devices_array, device_obj);
    
    json_object_set_new(root, "devices", devices_array);
    json_object_set_new(root, "timestamp", json_integer(time(NULL)));
    json_object_set_new(root, "single_request", json_boolean(1));
    
    char *json_str = json_dumps(root, JSON_COMPACT);
    json_decref(root);
    
    if (!json_str) {
        pthread_mutex_unlock(&config->ws_mutex);
        log_error("Failed to create JSON for device output");
        return;
    }
    
    // Отправляем всем клиентам
    websocket_client_t *client = config->ws_clients;
    websocket_client_t *prev = NULL;
    int active_clients = 0;
    
    while (client) {
        if (send_websocket_frame(client->fd, 0x1, json_str, strlen(json_str)) < 0) {
            // Ошибка отправки, закрываем соединение
            log_debug("WebSocket send failed, closing connection (fd: %d)", client->fd);
            
            if (prev) {
                prev->next = client->next;
            } else {
                config->ws_clients = client->next;
            }
            
            websocket_client_t *to_free = client;
            client = client->next;
            close(to_free->fd);
            free(to_free);
            continue;
        }
        
        client->last_active = time(NULL);
        active_clients++;
        prev = client;
        client = client->next;
    }
    
    pthread_mutex_unlock(&config->ws_mutex);
    
    if (config->log_level >= LOG_LEVEL_DEBUG && active_clients > 0) {
        log_debug("Sent device data to %d WebSocket clients (%zu bytes)",
                 active_clients, strlen(json_str));
    }
    
    free(json_str);
}

// Обработка входящей команды записи Modbus
void process_modbus_write_command(config_t *config, const char *json_data) {
    if (!json_data || config->websocket_port <= 0) {
        return;
    }
    
    log_debug("Processing Modbus write command: %s", json_data);
    
    // Парсим JSON
    json_error_t error;
    json_t *root = json_loads(json_data, 0, &error);
    if (!root) {
        log_error("Failed to parse write command JSON: %s", error.text);
        send_write_result_to_websocket(config, 0, "Invalid JSON format", NULL);
        return;
    }
    
    // Извлекаем поля
    json_t *value_obj = json_object_get(root, "value");
    json_t *fc_obj = json_object_get(root, "fc");
    json_t *unitid_obj = json_object_get(root, "unitid");
    json_t *address_obj = json_object_get(root, "address");
    json_t *quantity_obj = json_object_get(root, "quantity");
    
    if (!value_obj || !fc_obj || !unitid_obj || !address_obj || !quantity_obj) {
        log_error("Missing required fields in write command");
        send_write_result_to_websocket(config, 0, "Missing required fields (value, fc, unitid, address, quantity)", NULL);
        json_decref(root);
        return;
    }
    
    int value = json_integer_value(value_obj);
    int fc = json_integer_value(fc_obj);
    int unitid = json_integer_value(unitid_obj);
    int address = json_integer_value(address_obj);
    int quantity = json_integer_value(quantity_obj);
    
    // Проверяем функцию
    if (fc != 5 && fc != 6 && fc != 15 && fc != 16) {
        log_error("Unsupported function code: %d", fc);
        send_write_result_to_websocket(config, 0, "Unsupported function code (must be 5, 6, 15 or 16)", NULL);
        json_decref(root);
        return;
    }
    
    // Создаем команду для выполнения
    modbus_write_command_t cmd;
    cmd.value = value;
    cmd.function_code = fc;
    cmd.unit_id = unitid;
    cmd.address = address;
    cmd.quantity = quantity;
    cmd.values = NULL;
    
    // Для функций 15/16 может потребоваться массив значений
    if ((fc == 15 || fc == 16) && quantity > 1) {
        cmd.values = malloc(quantity * sizeof(uint16_t));
        if (!cmd.values) {
            log_error("Memory allocation failed for values array");
            send_write_result_to_websocket(config, 0, "Memory allocation failed", NULL);
            json_decref(root);
            return;
        }
        // Заполняем первым значением (для простоты)
        for (int i = 0; i < quantity; i++) {
            cmd.values[i] = (uint16_t)value;
        }
    }
    
    json_decref(root);
    
    // Выполняем команду немедленно
    execute_modbus_write_immediate(config, &cmd);
}

// Отправка результата записи в WebSocket
void send_write_result_to_websocket(config_t *config, int success, const char *error_msg, modbus_write_command_t *cmd) {
    if (config->websocket_port <= 0) {
        return;
    }
    
    pthread_mutex_lock(&config->ws_mutex);
    
    if (config->ws_clients == NULL) {
        pthread_mutex_unlock(&config->ws_mutex);
        return;
    }
    
    // Формируем JSON ответа
    json_t *root = json_object();
    json_object_set_new(root, "type", json_string("write_result"));
    json_object_set_new(root, "success", json_boolean(success));
    
    if (!success && error_msg) {
        json_object_set_new(root, "error", json_string(error_msg));
    } else {
        json_object_set_new(root, "error", json_null());
    }
    
    // Добавляем детали запроса
    if (cmd) {
        json_t *details = json_object();
        json_object_set_new(details, "unitid", json_integer(cmd->unit_id));
        json_object_set_new(details, "function", json_integer(cmd->function_code));
        json_object_set_new(details, "address", json_integer(cmd->address));
        json_object_set_new(details, "quantity", json_integer(cmd->quantity));
        json_object_set_new(root, "details", details);
    }
    
    json_object_set_new(root, "timestamp", json_integer(time(NULL)));
    
    char *json_str = json_dumps(root, JSON_COMPACT);
    json_decref(root);
    
    if (!json_str) {
        pthread_mutex_unlock(&config->ws_mutex);
        log_error("Failed to create JSON for write result");
        return;
    }
    
    // Отправляем всем клиентам
    websocket_client_t *client = config->ws_clients;
    websocket_client_t *prev = NULL;
    int active_clients = 0;
    
    while (client) {
        if (send_websocket_frame(client->fd, 0x1, json_str, strlen(json_str)) < 0) {
            log_debug("WebSocket send failed, closing connection (fd: %d)", client->fd);
            
            if (prev) {
                prev->next = client->next;
            } else {
                config->ws_clients = client->next;
            }
            
            websocket_client_t *to_free = client;
            client = client->next;
            close(to_free->fd);
            free(to_free);
            continue;
        }
        
        client->last_active = time(NULL);
        active_clients++;
        prev = client;
        client = client->next;
    }
    
    pthread_mutex_unlock(&config->ws_mutex);
    
    if (config->log_level >= LOG_LEVEL_DEBUG && active_clients > 0) {
        log_debug("Sent write result to %d WebSocket clients (%zu bytes)",
                 active_clients, strlen(json_str));
    }
    
    free(json_str);
}
