#!/usr/bin/env python3
import asyncio
import websockets
import json
import sys

async def test_websocket():
    uri = "ws://127.0.0.1:24123"
    
    try:
        async with websockets.connect(uri) as websocket:
            print(f"Connected to {uri}")
            
            # Получаем данные
            for i in range(5):  # Получаем 5 сообщений
                try:
                    message = await asyncio.wait_for(websocket.recv(), timeout=10.0)
                    data = json.loads(message)
                    
                    print(f"\n=== Message {i+1} ===")
                    print(f"Timestamp: {data.get('timestamp')}")
                    print(f"Devices: {len(data.get('devices', []))}")
                    
                    for device in data.get('devices', []):
                        addr = device.get('address')
                        available = device.get('available')
                        print(f"  Device {addr}: {'✓' if available else '✗'}")
                        
                except asyncio.TimeoutError:
                    print("Timeout waiting for message")
                    break
                    
    except ConnectionRefusedError:
        print(f"Connection refused to {uri}")
        print("Make sure mbusread service is running with WebSocket enabled")
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    asyncio.run(test_websocket())

