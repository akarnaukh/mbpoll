#!/bin/bash

echo "=== Monitoring mbusread service ==="
echo "Press Ctrl+C to stop monitoring"
echo ""

# Показываем статус
sudo systemctl status mbusread@coold1 --no-pager

echo ""
echo "=== Following logs (last 50 lines) ==="
sudo journalctl -u mbusread@coold1 -f -n 50
