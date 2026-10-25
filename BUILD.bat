@echo off
rem Build lokal di Windows. Prasyarat: Visual Studio 2022 (Desktop C++), CMake, Qt 6 (MSVC 64-bit).
rem   set QT_DIR=C:\Qt\6.7.2\msvc2019_64
rem Jalankan dari "x64 Native Tools Command Prompt" atau terminal biasa (CMake memilih VS otomatis).
cd /d "%~dp0"
if "%QT_DIR%"=="" (echo Set QT_DIR dulu, contoh: set QT_DIR=C:\Qt\6.7.2\msvc2019_64 & pause & exit /b 1)

if not exist ffmpeg\include (
    echo Mengunduh FFmpeg...
    curl.exe -L -o ffmpeg.zip https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-n8.1-latest-win64-lgpl-shared-8.1.zip || (pause & exit /b 1)
    tar -xf ffmpeg.zip || (pause & exit /b 1)
    for /d %%D in (ffmpeg-*) do move "%%D" ffmpeg >nul
    del ffmpeg.zip
)

cmake -S . -B build -A x64 -DCMAKE_PREFIX_PATH=%QT_DIR% -DFFMPEG_ROOT=%CD%\ffmpeg || (pause & exit /b 1)
cmake --build build --config Release || (pause & exit /b 1)

if exist dist rmdir /s /q dist
mkdir dist\CCTVMonitor
copy /y build\Release\CCTVMonitor.exe dist\CCTVMonitor\ >nul
copy /y ffmpeg\bin\*.dll dist\CCTVMonitor\ >nul
copy /y cameras.json dist\CCTVMonitor\ >nul
%QT_DIR%\bin\windeployqt.exe --release --no-translations dist\CCTVMonitor\CCTVMonitor.exe
echo.
echo Selesai: dist\CCTVMonitor\CCTVMonitor.exe  (folder bisa dipindah / dibuat shortcut)
pause
