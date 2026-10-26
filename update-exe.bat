@echo off
rem Ambil CCTVMonitor.exe terbaru dari branch latest-exe (hasil build GitHub Actions)
rem lalu timpa ke folder aplikasi. Tutup CCTV Monitor dulu.
rem Pakai: update-exe.bat   (opsional: set DEST=folder lain)
if "%DEST%"=="" set "DEST=C:\Users\ahmad\Downloads\CCTVMonitor-windows\CCTVMonitor-windows\CCTVMonitor"
set "TMPEXE=%TEMP%\CCTVMonitor-new.exe"

curl.exe -fL -o "%TMPEXE%" https://raw.githubusercontent.com/orang-sukses/cctv-monitor/latest-exe/CCTVMonitor.exe || (echo Gagal mengunduh. & pause & exit /b 1)
copy /y "%TMPEXE%" "%DEST%\CCTVMonitor.exe" >nul || (echo Gagal menimpa exe: tutup dulu CCTV Monitor. & del "%TMPEXE%" & pause & exit /b 1)
del "%TMPEXE%"
echo OK: %DEST%\CCTVMonitor.exe diperbarui
pause
