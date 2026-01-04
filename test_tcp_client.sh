#!/bin/bash

echo "=== Testing TCP client access ==="
echo "Connecting to mbusread TCP server on port 24122..."
echo ""

# Используем netcat для подключения
# Сначала читаем длину данных (4 байта), затем сами данные
(echo -n && sleep 0.1) | nc 0.0.0.0 24122 2>/dev/null | \
python3 -c "
import sys
import json
import struct

try:
    # Читаем 4 байта длины
    len_bytes = sys.stdin.buffer.read(4)
    if not len_bytes or len(len_bytes) != 4:
        print('No data received or invalid length')
        sys.exit(1)
    
    data_len = struct.unpack('!I', len_bytes)[0]
    print(f'Data length: {data_len} bytes')
    
    # Читаем данные
    data = sys.stdin.buffer.read(data_len)
    if len(data) != data_len:
        print(f'Incomplete data: got {len(data)} bytes, expected {data_len}')
        sys.exit(1)
    
    # Парсим JSON
    json_data = json.loads(data.decode('utf-8'))
    
    print('\n=== Parsed JSON data ===')
    print(f'Timestamp: {json_data.get(\"timestamp\")}')
    print(f'Number of devices: {len(json_data.get(\"devices\", []))}')
    
    for device in json_data.get('devices', []):
        print(f'\nDevice {device.get(\"address\")}:')
        print(f'  Available: {device.get(\"available\")}')
        for reg in device.get('registers', []):
            func = reg.get('function')
            start = reg.get('start')
            qty = reg.get('quantity')
            values = reg.get('values', [])
            print(f'  Function {func} (registers {start}-{start+qty-1}):')
            
            # Показываем значения
            if all(isinstance(v, int) for v in values):
                # Числовые значения
                hex_values = ' '.join(f'{v:04X}' for v in values[:5])
                if len(values) > 5:
                    hex_values += f' ... (+{len(values)-5} more)'
                print(f'    Values (hex): {hex_values}')
                
                dec_values = ' '.join(str(v) for v in values[:5])
                if len(values) > 5:
                    dec_values += f' ... (+{len(values)-5} more)'
                print(f'    Values (dec): {dec_values}')
            else:
                # Строковые значения (например, "na")
                str_values = ' '.join(str(v) for v in values[:5])
                if len(values) > 5:
                    str_values += f' ... (+{len(values)-5} more)'
                print(f'    Values: {str_values}')
    
except Exception as e:
    print(f'Error: {e}')
    sys.exit(1)
"
