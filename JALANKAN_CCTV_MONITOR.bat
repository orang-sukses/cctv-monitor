@echo off
chcp 65001 >nul
title CCTV Monitor - Multi-View NVR Station
color 0B

echo.
echo ======================================================================
echo       🛰️  CCTV MONITOR - MULTI-VIEW NVR STATION (GRID 3x3) 🛰️
echo             Mode Default 360p (Hemat Bandwidth ^& Ringan)
echo ======================================================================
echo.
echo  [1/2] Memeriksa runtime Python di sistem Windows Anda...

cd /d "%~dp0"

:: 1. Cek Python di PATH
where python >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    echo  [OK] Python terdeteksi di PATH.
    echo  [2/2] Server aktif di: http://localhost:8080
    echo.
    echo  Server CCTV Monitor sedang aktif!
    echo  Buka browser Anda ke http://localhost:8080
    echo  Jangan tutup jendela ini selama memantau CCTV.
    echo ----------------------------------------------------------------------
    python server.py
    goto finish
)

:: 2. Cek Python 3.12 di AppData pengguna
if exist "%LOCALAPPDATA%\Programs\Python\Python312\python.exe" (
    echo  [OK] Python 3.12 terdeteksi di AppData.
    echo  [2/2] Server aktif di: http://localhost:8080
    echo.
    echo  Server CCTV Monitor sedang aktif!
    echo  Buka browser Anda ke http://localhost:8080
    echo  Jangan tutup jendela ini selama memantau CCTV.
    echo ----------------------------------------------------------------------
    "%LOCALAPPDATA%\Programs\Python\Python312\python.exe" server.py
    goto finish
)

:: 3. Cek Python 3.11 di AppData pengguna
if exist "%LOCALAPPDATA%\Programs\Python\Python311\python.exe" (
    echo  [OK] Python 3.11 terdeteksi di AppData.
    echo  [2/2] Server aktif di: http://localhost:8080
    echo.
    echo  Server CCTV Monitor sedang aktif!
    echo  Buka browser Anda ke http://localhost:8080
    echo  Jangan tutup jendela ini selama memantau CCTV.
    echo ----------------------------------------------------------------------
    "%LOCALAPPDATA%\Programs\Python\Python311\python.exe" server.py
    goto finish
)

echo.
echo  [ERROR] Tidak ditemukan Python di sistem Windows!
echo  Silakan pasang Python dari https://python.org (centang 'Add Python to PATH')
echo.
pause
:finish
