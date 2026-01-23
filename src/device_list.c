#include "device_list.h"
//#include "daemon.h"
//#include <ctype.h>

int parse_address_range(const char *str, int *start, int *end) {
    char *dash = strchr(str, '-');
    if (dash) {
        *dash = '\0';
        *start = atoi(str);
        *end = atoi(dash + 1);
        *dash = '-'; // Восстанавливаем строку
        return 1;
    } else {
        *start = atoi(str);
        *end = *start;
        return 0;
    }
}

int load_device_list(config_t *config) {
    log_info("Loading device list from: %s", config->device_list_file);
    
    // Проверяем существование файла
    if (access(config->device_list_file, R_OK) != 0) {
        log_error("Device list file not found or not readable: %s", config->device_list_file);
        return -1;
    }
    
    log_debug("Parsing JSON file...");
    json_error_t error;
    json_t *root = json_load_file(config->device_list_file, 0, &error);
    
    if (!root) {
        log_error("JSON parse error at line %d: %s", error.line, error.text);
        return -1;
    }
    
    log_debug("JSON parsed successfully");
    
    json_t *devices = json_object_get(root, "devices");
    if (!devices || !json_is_object(devices)) {
        log_error("Invalid device list format: missing 'devices' object");
        json_decref(root);
        return -1;
    }
    
    log_debug("Found 'devices' object");
    
    // Сначала подсчитаем общее количество устройств
    config->device_count = 0;
    const char *key;
    json_t *value;
    
    log_debug("Counting devices...");
    json_object_foreach(devices, key, value) {
        int start, end;
        parse_address_range(key, &start, &end);
        int devices_in_range = (end - start + 1);
        config->device_count += devices_in_range;
        log_debug("  Range '%s': %d devices (from %d to %d)", key, devices_in_range, start, end);
    }
    
    log_info("Total devices to load: %d", config->device_count);
    
    if (config->device_count == 0) {
        log_warn("No devices found in device list");
        json_decref(root);
        return 0; // Это не ошибка, просто нет устройств
    }
    
    // Выделяем память для устройств
    log_debug("Allocating memory for %d devices...", config->device_count);
    config->devices = malloc(config->device_count * sizeof(modbus_device_t));
    if (!config->devices) {
        log_error("Memory allocation failed for %d devices", config->device_count);
        json_decref(root);
        return -1;
    }
    
    memset(config->devices, 0, config->device_count * sizeof(modbus_device_t));
    
    // Заполняем структуры устройств
    log_debug("Processing device definitions...");
    int device_index = 0;
    json_object_foreach(devices, key, value) {
        int start_addr, end_addr;
        int is_range = parse_address_range(key, &start_addr, &end_addr);
        
        if (is_range) {
            log_debug("Processing device range: %d-%d", start_addr, end_addr);
            for (int addr = start_addr; addr <= end_addr; addr++) {
                modbus_device_t *device = &config->devices[device_index++];
                device->slave_address = addr;
                device->device_available = 1;
                
                // Подсчитываем количество диапазонов регистров
                const char *func_key;
                json_t *func_value;
                device->range_count = 0;
                
                json_object_foreach(value, func_key, func_value) {
                    device->range_count++;
                }
                
                log_debug("  Device %d: %d register ranges", addr, device->range_count);
                
                // Выделяем память для диапазонов
                device->ranges = malloc(device->range_count * sizeof(register_range_t));
                if (!device->ranges) {
                    log_error("Memory allocation failed for device %d ranges", addr);
                    json_decref(root);
                    return -1;
                }
                
                // Заполняем диапазоны
                int range_index = 0;
                json_object_foreach(value, func_key, func_value) {
                    register_range_t *range = &device->ranges[range_index++];
                    range->address = addr;
                    range->function = atoi(func_key);
                    
                    json_t *start_json = json_object_get(func_value, "s");
                    json_t *qty_json = json_object_get(func_value, "q");
                    
                    if (start_json && qty_json) {
                        range->start = json_integer_value(start_json);
                        range->quantity = json_integer_value(qty_json);
                        range->values = NULL; // Будет выделено при чтении
                        
                        log_info("  Device %d: Function %d - start: %d, quantity: %d", 
                                addr, range->function, range->start, range->quantity);
                    } else {
                        log_error("Invalid range definition for device %d: missing 's' or 'q'", addr);
                    }
                }
            }
        } else {
            log_debug("Processing single device: %d", start_addr);
            modbus_device_t *device = &config->devices[device_index++];
            device->slave_address = start_addr;
            device->device_available = 1;
            
            // Подсчитываем количество диапазонов регистров
            const char *func_key;
            json_t *func_value;
            device->range_count = 0;
            
            json_object_foreach(value, func_key, func_value) {
                device->range_count++;
            }
            
            log_debug("  Device %d: %d register ranges", start_addr, device->range_count);
            
            // Выделяем память для диапазонов
            device->ranges = malloc(device->range_count * sizeof(register_range_t));
            if (!device->ranges) {
                log_error("Memory allocation failed for device %d ranges", start_addr);
                json_decref(root);
                return -1;
            }
            
            // Заполняем диапазоны
            int range_index = 0;
            json_object_foreach(value, func_key, func_value) {
                register_range_t *range = &device->ranges[range_index++];
                range->address = start_addr;
                range->function = atoi(func_key);
                
                json_t *start_json = json_object_get(func_value, "s");
                json_t *qty_json = json_object_get(func_value, "q");
                
                if (start_json && qty_json) {
                    range->start = json_integer_value(start_json);
                    range->quantity = json_integer_value(qty_json);
                    range->values = NULL; // Будет выделено при чтении
                    
                    log_info("  Device %d: Function %d - start: %d, quantity: %d", 
                            start_addr, range->function, range->start, range->quantity);
                } else {
                    log_error("Invalid range definition for device %d: missing 's' or 'q'", start_addr);
                }
            }
        }
    }
    
    json_decref(root);
    
    log_info("Successfully loaded %d devices", config->device_count);
    return 0;
}

