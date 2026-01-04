#!/bin/bash

echo "=== Checking libmodbus installation ==="

# Проверяем наличие библиотеки
echo "1. Checking if libmodbus is installed..."
if pkg-config --exists libmodbus; then
    version=$(pkg-config --modversion libmodbus)
    echo "   libmodbus version: $version"
else
    echo "   libmodbus not found via pkg-config"
fi

echo ""
echo "2. Checking library files..."
ldconfig -p | grep modbus

echo ""
echo "3. Checking header files..."
ls -la /usr/include/modbus/ 2>/dev/null || ls -la /usr/local/include/modbus/ 2>/dev/null || echo "   modbus headers not found"

echo ""
echo "4. Testing modbus_set_tcp_keepalive function..."
cat > /tmp/test_keepalive.c << 'TEST'
#include <modbus/modbus.h>
int main() {
    modbus_t *ctx = modbus_new_tcp("127.0.0.1", 502);
    if (ctx) {
        // Проверяем наличие функции
        #ifdef modbus_set_tcp_keepalive
        printf("modbus_set_tcp_keepalive is available\n");
        #else
        printf("modbus_set_tcp_keepalive is NOT available\n");
        #endif
        modbus_free(ctx);
    }
    return 0;
}
TEST

gcc -o /tmp/test_keepalive /tmp/test_keepalive.c -lmodbus 2>/dev/null
if [ $? -eq 0 ]; then
    /tmp/test_keepalive
else
    echo "   Failed to compile test program"
fi

rm -f /tmp/test_keepalive /tmp/test_keepalive.c

echo ""
echo "=== Checking current mbusread build ==="
#make check-libmodbus
