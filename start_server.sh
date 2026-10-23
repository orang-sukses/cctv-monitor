#!/bin/bash
cd "$(dirname "$0")"

# Bersihkan proses lama jika port 8080 masih terpakai
OLD_PID=$(lsof -ti:8080 2>/dev/null || fuser 8080/tcp 2>/dev/null | tr -d ' ')
if [ -n "$OLD_PID" ]; then
    echo "Menutup proses lama di port 8080 (PID: $OLD_PID)..."
    kill -9 $OLD_PID 2>/dev/null
    sleep 0.5
fi

echo "=========================================================="
echo "  🛰️  CCTV Monitor - Multi-View NVR Station             "
echo "  📺 Mode Default 360p (Hemat Bandwidth & Ringan)          "
echo "  🌐 Server aktif di: http://localhost:8080               "
echo "  Buka link di browser: http://localhost:8080             "
echo "  Tekan Ctrl+C untuk menghentikan server                 "
echo "=========================================================="
python3 server.py
