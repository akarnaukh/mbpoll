#include "tcp_server.h"
#include "daemon.h"
#include <sys/select.h>

void *tcp_server_thread(void *arg) {
    config_t *config = (config_t *)arg;
    
    log_info("TCP server thread starting on %s:%d", 
             config->listing_ip, config->listing_port);
    
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        log_error("TCP server: socket creation failed: %s", strerror(errno));
        return NULL;
    }
    
    // Позволяем повторно использовать адрес
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        log_error("TCP server: setsockopt failed: %s", strerror(errno));
        close(server_fd);
        return NULL;
    }
    
    // Устанавливаем неблокирующий режим для корректного выхода
    int flags = fcntl(server_fd, F_GETFL, 0);
    fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);
    
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(config->listing_ip);
    address.sin_port = htons(config->listing_port);
    
    log_debug("TCP server: binding to %s:%d", config->listing_ip, config->listing_port);
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        log_error("TCP server: bind failed: %s", strerror(errno));
        close(server_fd);
        return NULL;
    }
    
    log_debug("TCP server: listening...");
    if (listen(server_fd, 5) < 0) {
        log_error("TCP server: listen failed: %s", strerror(errno));
        close(server_fd);
        return NULL;
    }
    
    log_info("TCP server listening on %s:%d", config->listing_ip, config->listing_port);
    
    while (g_running) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);
        
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        
        int activity = select(server_fd + 1, &read_fds, NULL, NULL, &tv);
        
        if (activity < 0 && errno != EINTR) {
            log_error("TCP server: select error: %s", strerror(errno));
            break;
        }
        
        if (!g_running) {
            log_debug("TCP server: received shutdown signal");
            break;
        }
        
        if (activity > 0 && FD_ISSET(server_fd, &read_fds)) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            
            int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
            if (client_fd < 0) {
                if (errno != EWOULDBLOCK && errno != EAGAIN) {
                    log_error("TCP server: accept failed: %s", strerror(errno));
                }
                continue;
            }
            
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
            
            log_info("TCP client connected from %s:%d", 
                     client_ip, ntohs(client_addr.sin_port));
            
            // Отправляем данные клиенту
            send_data_to_client(client_fd, config);
            
            close(client_fd);
            log_debug("TCP client disconnected");
        }
    }
    
    close(server_fd);
    log_info("TCP server stopped");
    
    return NULL;
}

void send_data_to_client(int client_fd, config_t *config) {
    pthread_mutex_lock(&g_data_mutex);
    
    log_debug("Sending data to TCP client");
    
    // Формируем JSON ответ
    json_t *root = json_object();
    json_t *devices_array = json_array();
    
    for (int i = 0; i < config->device_count; i++) {
        modbus_device_t *device = &config->devices[i];
        
        json_t *device_obj = json_object();
        json_object_set_new(device_obj, "address", json_integer(device->slave_address));
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
    }
    
    json_object_set_new(root, "devices", devices_array);
    json_object_set_new(root, "timestamp", json_integer(time(NULL)));
    
    char *json_str = json_dumps(root, JSON_COMPACT);
    json_decref(root);
    
    if (json_str) {
        // Отправляем длину данных
        size_t len = strlen(json_str);
        uint32_t net_len = htonl(len);
        
        log_debug("Sending %zu bytes to TCP client", len);
        
        if (write(client_fd, &net_len, sizeof(net_len)) == sizeof(net_len)) {
            // Отправляем JSON данные
            if (write(client_fd, json_str, len) == (ssize_t)len) {
                log_debug("Data sent successfully to TCP client");
            } else {
                log_error("Failed to send data to TCP client: %s", strerror(errno));
            }
        } else {
            log_error("Failed to send length to TCP client: %s", strerror(errno));
        }
        
        free(json_str);
    } else {
        log_error("Failed to create JSON string");
    }
    
    pthread_mutex_unlock(&g_data_mutex);
}

