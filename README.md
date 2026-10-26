# cctv-monitoring

Aplikasi **desktop Windows** untuk menampilkan banyak kamera CCTV (RTSP) sekaligus.
C++17 + Qt6 + FFmpeg. Tanpa web server, tanpa browser, tanpa Python. Latency rendah, hemat CPU.

## Pakai (tanpa build)
Ambil `CCTVMonitor-windows.zip` dari tab **Actions** (artifact) atau **Releases** repo GitHub, ekstrak,
edit `cameras.json`, lalu klik 2x `CCTVMonitor.exe`.

## Pengaturan kamera (`cameras.json`)
```json
{ "max_fps": 0, "hwaccel": true,
  "cameras": [ { "name": "CAM 01", "url": "rtsp://admin:@192.168.1.4:554/live/ch0",
                 "sub_url": "rtsp://admin:@192.168.1.4:554/live/ch1" } ] }
```
- `max_fps`: batas fps tampil (0 = tanpa batas)
- `hwaccel`: decode GPU (D3D11VA), otomatis fallback ke CPU
- `overlay` (opsional): tampilan teks nama/status kamera — `position` ("bottom"/"top"), `font`, `size`, `bold`, `name_color`, `live_color`, `warn_color` (warna hex), `bar_opacity` (0-255), `show_name`, `show_status`
- `quality`: `"sd"` (default, memakai `sub_url`, atau `url` bila kosong) atau `"hd"` (memakai `url` utama); bisa diganti saat berjalan dengan tombol `H`
- `sub_url` (opsional): stream ringan untuk mode SD
- Overlay tiap kamera menampilkan total data internet yang terpakai; judul jendela menampilkan total semua kamera dan mode SD/HD

## Kontrol
`1`-`9` / klik 2x kamera: perbesar · `0`/`Esc`: kembali ke grid · `R`: rekam semua (folder `recordings/`, per 10 menit, tanpa re-encode) · `S`: snapshot (`snapshots/`) · `H`: ganti SD/HD · `F`/`F11`: fullscreen (default-nya jendela biasa; jalankan dengan `--fullscreen` untuk langsung fullscreen) · `Q`: keluar

## Build sendiri
- **GitHub Actions**: push ke `main` -> workflow `Build Windows` menghasilkan zip siap pakai.
- **Lokal**: install Visual Studio 2022 (Desktop C++), CMake, Qt 6 (MSVC 64-bit); `set QT_DIR=C:\Qt\6.7.2\msvc2019_64`; jalankan `BUILD.bat` -> `dist\CCTVMonitor\`.

## Struktur kode (`src/`)
| Kelas | Tugas |
|---|---|
| `AppConfig` | baca `cameras.json` |
| `StreamInput` | koneksi RTSP (TCP, tanpa buffer, timeout, bisa dibatalkan) |
| `VideoDecoder` | decode GPU/CPU, keyframe-only bila tersembunyi |
| `LagMonitor` | deteksi decode tertinggal -> lompat ke keyframe terbaru |
| `FrameScaler` / `FrameBuffer` | skala+RGB sekali jalan, double buffer tanpa alokasi per frame |
| `Recorder` | remux ke .mkv per 10 menit |
| `CameraWorker` | satu thread per kamera, auto-reconnect |
| `CameraView` / `MainWindow` | tampilan grid/solo, kontrol keyboard |
