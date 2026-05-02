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
    const char *name_key;
    json_t *name_value;
    
    log_debug("Counting devices...");
    json_object_foreach(devices, name_key, name_value) {
        if (!json_is_object(name_value)) {
            log_warn("Device '%s' is not an object, skipping", name_key);
            continue;
        }
        
        // Внутри имени устройства ищем адреса
        json_t *addr_obj = json_object_get(name_value, "address");
        if (!addr_obj) {
            log_warn("Device '%s' has no 'address' field, skipping", name_key);
            continue;
        }
        
        const char *addr_key = json_string_value(addr_obj);
        if (!addr_key) {
            log_warn("Device '%s' has invalid 'address' field, skipping", name_key);
            continue;
        }
        
        int start, end;
        parse_address_range(addr_key, &start, &end);
        int devices_in_range = (end - start + 1);
        config->device_count += devices_in_range;
        log_debug("  Device '%s', address range '%s': %d devices (from %d to %d)", 
                  name_key, addr_key, devices_in_range, start, end);
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
    json_object_foreach(devices, name_key, name_value) {
        if (!json_is_object(name_value)) {
            continue;
        }
        
        json_t *addr_obj = json_object_get(name_value, "address");
        if (!addr_obj) {
            continue;
        }
        
        const char *addr_key = json_string_value(addr_obj);
        if (!addr_key) {
            continue;
        }
        
        int start_addr, end_addr;
        int is_range = parse_address_range(addr_key, &start_addr, &end_addr);
        
        // Получаем диапазоны регистров из объекта ranges
        json_t *ranges_obj = json_object_get(name_value, "ranges");
        if (!ranges_obj || !json_is_object(ranges_obj)) {
            log_warn("Device '%s' has no 'ranges' object, skipping", name_key);
            continue;
        }
        
        if (is_range) {
            log_debug("Processing device range: %d-%d", start_addr, end_addr);
            for (int addr = start_addr; addr <= end_addr; addr++) {
                modbus_device_t *device = &config->devices[device_index++];
                device->name = strdup(name_key);
                device->slave_address = addr;
                device->device_available = 1;
                
                // Подсчитываем количество диапазонов регистров
                const char *func_key;
                json_t *func_value;
                device->range_count = 0;
                
                json_object_foreach(ranges_obj, func_key, func_value) {
                    device->range_count++;
                }
                
                log_debug("  Device '%s' addr %d: %d register ranges", name_key, addr, device->range_count);
                
                // Выделяем память для диапазонов
                device->ranges = malloc(device->range_count * sizeof(register_range_t));
                if (!device->ranges) {
                    log_error("Memory allocation failed for device %d ranges", addr);
                    json_decref(root);
                    return -1;
                }
                
                // Заполняем диапазоны
                int range_index = 0;
                json_object_foreach(ranges_obj, func_key, func_value) {
                    register_range_t *range = &device->ranges[range_index++];
                    range->address = addr;
                    range->function = atoi(func_key);
                    
                    json_t *start_json = json_object_get(func_value, "s");
                    json_t *qty_json = json_object_get(func_value, "q");
                    
                    if (start_json && qty_json) {
                        range->start = json_integer_value(start_json);
                        range->quantity = json_integer_value(qty_json);
                        range->values = NULL; // Будет выделено при чтении
                        
                        log_info("  Device '%s' addr %d: Function %d - start: %d, quantity: %d", 
                                name_key, addr, range->function, range->start, range->quantity);
                    } else {
                        log_error("Invalid range definition for device '%s' addr %d: missing 's' or 'q'", 
                                  name_key, addr);
                    }
                }
            }
        } else {
            log_debug("Processing single device: %d", start_addr);
            modbus_device_t *device = &config->devices[device_index++];
            device->name = strdup(name_key);
            device->slave_address = start_addr;
            device->device_available = 1;
            
            // Подсчитываем количество диапазонов регистров
            const char *func_key;
            json_t *func_value;
            device->range_count = 0;
            
            json_object_foreach(ranges_obj, func_key, func_value) {
                device->range_count++;
            }
            
            log_debug("  Device '%s' addr %d: %d register ranges", name_key, start_addr, device->range_count);
            
            // Выделяем память для диапазонов
            device->ranges = malloc(device->range_count * sizeof(register_range_t));
            if (!device->ranges) {
                log_error("Memory allocation failed for device %d ranges", start_addr);
                json_decref(root);
                return -1;
            }
            
            // Заполняем диапазоны
            int range_index = 0;
            json_object_foreach(ranges_obj, func_key, func_value) {
                register_range_t *range = &device->ranges[range_index++];
                range->address = start_addr;
                range->function = atoi(func_key);
                
                json_t *start_json = json_object_get(func_value, "s");
                json_t *qty_json = json_object_get(func_value, "q");
                
                if (start_json && qty_json) {
                    range->start = json_integer_value(start_json);
                    range->quantity = json_integer_value(qty_json);
                    range->values = NULL; // Будет выделено при чтении
                    
                    log_info("  Device '%s' addr %d: Function %d - start: %d, quantity: %d", 
                            name_key, start_addr, range->function, range->start, range->quantity);
                } else {
                    log_error("Invalid range definition for device '%s' addr %d: missing 's' or 'q'", 
                              name_key, start_addr);
                }
            }
        }
    }
    
    json_decref(root);
    
    log_info("Successfully loaded %d devices", config->device_count);
    return 0;
}

