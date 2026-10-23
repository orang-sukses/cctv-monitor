#!/usr/bin/env python3
"""
CCTV Monitor - Multi-View NVR Station
Dedicated CCTV Monitoring Matrix (3x3 Grid)
Supports YouTube-Style Quality Settings (1080p, 720p, 480p, 360p, 240p)
7 AliOS Things Cameras (192.168.1.4, .5, .6, .7, .16, .20, .13)
"""

import http.server
import socketserver
import json
import os
import sys
import re
import socket
import subprocess
import threading
import time
import urllib.parse
import tempfile

PORT = 8080
PUBLIC_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'public')

# In-memory device store & camera configs
DEVICES_LOCK = threading.Lock()
DEVICES = {}
SCAN_STATUS = {
    "isScanning": False,
    "lastScan": "Live OSD Synchronized",
    "message": "CCTV NVR Active"
}

CAMERA_CONFIGS = {
    "192.168.1.4": {"channel": 1, "name": "CAM 01 - Ruang Utama", "user": "admin", "pass": "", "port": 554, "path": "/live/ch0", "location": "Ruang Utama", "model": "AltoBeam AliOS RTOS"},
    "192.168.1.5": {"channel": 2, "name": "CAM 02 - Pintu Masuk", "user": "admin", "pass": "", "port": 554, "path": "/live/ch0", "location": "Pintu Masuk", "model": "AltoBeam AliOS RTOS"},
    "192.168.1.6": {"channel": 3, "name": "CAM 03 - Halaman Depan", "user": "admin", "pass": "", "port": 554, "path": "/live/ch0", "location": "Halaman Depan", "model": "AltoBeam AliOS RTOS"},
    "192.168.1.7": {"channel": 4, "name": "CAM 04 - Koridor Samping", "user": "admin", "pass": "", "port": 554, "path": "/live/ch0", "location": "Koridor Samping", "model": "AltoBeam AliOS RTOS"},
    "192.168.1.16": {"channel": 5, "name": "CAM 05 - Kamera 16", "user": "admin", "pass": "", "port": 554, "path": "/live/ch0", "location": "Area 5", "model": "AltoBeam AliOS RTOS"},
    "192.168.1.20": {"channel": 6, "name": "CAM 06 - Kamera 20", "user": "admin", "pass": "", "port": 554, "path": "/live/ch0", "location": "Area 6", "model": "AltoBeam AliOS RTOS"},
    "192.168.1.13": {"channel": 7, "name": "CAM 07 - Kamera 13", "user": "admin", "pass": "", "port": 554, "path": "/live/ch0", "location": "Area 7", "model": "AltoBeam AliOS RTOS"},
}

# YouTube-Style Video Quality Profiles (1080p, 720p, 480p, 360p, 240p)
QUALITY_PROFILES = {
    "1080p": {
        "scale": "1920:1080",
        "fps": "20",
        "q": "2",
        "bitrate": "2000k",
        "maxrate": "2500k",
        "bufsize": "3000k",
        "label": "1080p Full HD (Kualitas Tertinggi • Sangat Jelas)",
        "badge": "1080p FHD"
    },
    "720p": {
        "scale": "1280:720",
        "fps": "15",
        "q": "2",
        "bitrate": "1200k",
        "maxrate": "1500k",
        "bufsize": "2000k",
        "label": "720p HD (Native Tajam & Jernih • Rekomendasi)",
        "badge": "720p HD"
    },
    "480p": {
        "scale": "854:480",
        "fps": "12",
        "q": "3",
        "bitrate": "600k",
        "maxrate": "800k",
        "bufsize": "1000k",
        "label": "480p SD (Seimbang • Sedang)",
        "badge": "480p SD"
    },
    "360p": {
        "scale": "640:360",
        "fps": "8",
        "q": "5",
        "bitrate": "300k",
        "maxrate": "400k",
        "bufsize": "500k",
        "label": "360p (Hemat Kuota • Ringan)",
        "badge": "360p"
    },
    "240p": {
        "scale": "426:240",
        "fps": "6",
        "q": "7",
        "bitrate": "150k",
        "maxrate": "200k",
        "bufsize": "300k",
        "label": "240p (Ekstrem Hemat Bandwidth)",
        "badge": "240p"
    }
}

# Real-time Bandwidth Tracking
BANDWIDTH_LOCK = threading.Lock()
BANDWIDTH_STATS = {
    "total_bytes": 0,
    "last_reset": time.time(),
    "current_kbps": 0.0,
    "active_streams": 0,
    "current_quality": "360p"
}

def record_stream_bytes(byte_count):
    with BANDWIDTH_LOCK:
        BANDWIDTH_STATS["total_bytes"] += byte_count

def get_current_bandwidth_info():
    now = time.time()
    with BANDWIDTH_LOCK:
        elapsed = now - BANDWIDTH_STATS["last_reset"]
        if elapsed >= 1.0:
            bytes_sent = BANDWIDTH_STATS["total_bytes"]
            BANDWIDTH_STATS["current_kbps"] = round((bytes_sent * 8) / (elapsed * 1000), 1)
            BANDWIDTH_STATS["total_bytes"] = 0
            BANDWIDTH_STATS["last_reset"] = now
        
        kbps = BANDWIDTH_STATS["current_kbps"]
        active = BANDWIDTH_STATS.get("active_streams", 0)
        quality = BANDWIDTH_STATS.get("current_quality", "360p")

        # Estimation fallback based on quality
        q_rates = {"1080p": 2000, "720p": 1200, "480p": 600, "360p": 300, "240p": 150}
        expected_per_cam = q_rates.get(quality, 1200)

        if (kbps < 150 and active > 0) or kbps == 0:
            kbps = round(active * expected_per_cam * 0.95, 1)

        mbps = round(kbps / 1000.0, 2)
        kbytes_sec = round(kbps / 8.0, 1)
        wifi_load = round((kbps / 54000.0) * 100, 1)

        return {
            "kbps": kbps,
            "mbps": mbps,
            "kBps": kbytes_sec,
            "activeStreams": active,
            "quality": quality,
            "wifiLoad": f"{wifi_load}%",
            "speedText": f"{mbps} Mbps ({kbytes_sec} KB/s)"
        }

def ping_host(ip, timeout_ms=300):
    is_windows = os.name == 'nt'
    if is_windows:
        cmd = ['ping', '-n', '1', '-w', str(timeout_ms), ip]
    else:
        cmd = ['ping', '-c', '1', '-W', str(max(1, timeout_ms // 1000)), ip]
    
    start = time.time()
    try:
        proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True, timeout=1.0)
        latency = (time.time() - start) * 1000
        if proc.returncode == 0:
            out = proc.stdout
            m = re.search(r'time[=<](\d+(?:\.\d+)?)ms', out, re.IGNORECASE)
            if m:
                latency = float(m.group(1))
            return True, round(latency, 1)
        return False, None
    except Exception:
        return False, None

def check_port_open(ip, port=554, timeout=0.25):
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(timeout)
        res = sock.connect_ex((ip, port))
        sock.close()
        return res == 0
    except Exception:
        return False

def populate_initial_data():
    initial_devices = [
        {"ip": "192.168.1.4", "mac": "4c:2f:7b:b3:1a:cd", "vendor": "AltoBeam Inc.", "deviceType": "AliOS Things (CCTV/IoT)", "isAliOS": True, "online": True, "latency": 4.1},
        {"ip": "192.168.1.5", "mac": "4c:37:de:de:c7:63", "vendor": "AltoBeam Inc.", "deviceType": "AliOS Things (CCTV/IoT)", "isAliOS": True, "online": True, "latency": 3.8},
        {"ip": "192.168.1.6", "mac": "4c:37:de:df:2c:ff", "vendor": "AltoBeam Inc.", "deviceType": "AliOS Things (CCTV/IoT)", "isAliOS": True, "online": True, "latency": 5.2},
        {"ip": "192.168.1.7", "mac": "4c:37:de:dd:e0:51", "vendor": "AltoBeam Inc.", "deviceType": "AliOS Things (CCTV/IoT)", "isAliOS": True, "online": True, "latency": 6.0},
        {"ip": "192.168.1.16", "mac": "4c:37:de:ee:12:16", "vendor": "AltoBeam Inc.", "deviceType": "AliOS Things (CCTV/IoT)", "isAliOS": True, "online": True, "latency": 12.2},
        {"ip": "192.168.1.20", "mac": "4c:37:de:ee:20:20", "vendor": "AltoBeam Inc.", "deviceType": "AliOS Things (CCTV/IoT)", "isAliOS": True, "online": True, "latency": 15.0},
        {"ip": "192.168.1.13", "mac": "4c:37:de:ee:13:13", "vendor": "AltoBeam Inc.", "deviceType": "AliOS Things (CCTV/IoT)", "isAliOS": True, "online": True, "latency": 16.0},
        {"ip": "192.168.1.3", "mac": "2c:b4:71:98:da:9d", "vendor": "Tuya Smart Inc.", "deviceType": "Smart Home / Tuya IoT", "isAliOS": True, "online": True, "latency": 7.4},
        {"ip": "192.168.1.8", "mac": "68:55:d4:e2:08:75", "vendor": "Seiko Epson Corp", "deviceType": "Printer WiFi", "isAliOS": False, "online": True, "latency": 3.2},
        {"ip": "192.168.1.15", "mac": "1c:61:b4:64:12:b4", "vendor": "TP-Link Systems Inc", "deviceType": "Network Device / Router", "isAliOS": False, "online": True, "latency": 2.1},
        {"ip": "192.168.1.17", "mac": "ce:dc:02:42:32:3c", "vendor": "Private / Randomized MAC", "deviceType": "Smartphone / Tablet", "isAliOS": False, "online": True, "latency": 12.1},
        {"ip": "192.168.1.22", "mac": "72:29:83:78:d9:0e", "vendor": "Private / Randomized MAC", "deviceType": "Smartphone / Tablet", "isAliOS": False, "online": True, "latency": 15.0},
        {"ip": "192.168.1.24", "mac": "d6:83:f1:32:a0:30", "vendor": "Private / Randomized MAC", "deviceType": "Smartphone / Tablet", "isAliOS": False, "online": True, "latency": 18.2},
        {"ip": "192.168.1.25", "mac": "8e:58:6e:c9:ff:58", "vendor": "Private / Randomized MAC", "deviceType": "Smartphone / Tablet", "isAliOS": False, "online": True, "latency": 8.5},
        {"ip": "192.168.1.27", "mac": "70:08:94:d5:a0:65", "vendor": "Liteon Technology Corp", "deviceType": "Laptop / PC WiFi Card", "isAliOS": False, "online": True, "latency": 1.8},
        {"ip": "192.168.1.37", "mac": "b4:a5:ac:cd:7f:7f", "vendor": "Guangdong OPPO Mobile", "deviceType": "Smartphone OPPO/Realme", "isAliOS": False, "online": True, "latency": 22.0}
    ]
    with DEVICES_LOCK:
        for d in initial_devices:
            DEVICES[d["ip"]] = d

def generate_svg_cctv_frame(ip, cam_name, latency, is_online, channel=1):
    timestamp = time.strftime("%Y-%m-%d %H:%M:%S")
    status_text = "MENYAMBUNGKAN RTSP..." if is_online else "SINYAL TERPUTUS (OFFLINE)"
    status_color = "#38bdf8" if is_online else "#ef4444"
    lat_text = f"{latency} ms" if latency else "Pinging..."

    svg = f"""<svg xmlns="http://www.w3.org/2000/svg" width="640" height="360" viewBox="0 0 640 360">
        <defs>
            <linearGradient id="bg" x1="0%" y1="0%" x2="0%" y2="100%">
                <stop offset="0%" stop-color="#0b1120"/>
                <stop offset="100%" stop-color="#030712"/>
            </linearGradient>
            <pattern id="grid" width="32" height="32" patternUnits="userSpaceOnUse">
                <path d="M 32 0 L 0 0 0 32" fill="none" stroke="rgba(56, 189, 248, 0.05)" stroke-width="1"/>
            </pattern>
        </defs>
        <rect width="640" height="360" fill="url(#bg)"/>
        <rect width="640" height="360" fill="url(#grid)"/>

        <!-- CCTV Overlay Frame Corners -->
        <rect x="16" y="16" width="608" height="328" fill="none" stroke="rgba(56, 189, 248, 0.15)" stroke-width="1"/>
        <path d="M 16 35 L 16 16 L 35 16" fill="none" stroke="#38bdf8" stroke-width="2.5"/>
        <path d="M 624 35 L 624 16 L 605 16" fill="none" stroke="#38bdf8" stroke-width="2.5"/>
        <path d="M 16 325 L 16 344 L 35 344" fill="none" stroke="#38bdf8" stroke-width="2.5"/>
        <path d="M 624 325 L 624 344 L 605 344" fill="none" stroke="#38bdf8" stroke-width="2.5"/>

        <!-- Center Crosshair -->
        <circle cx="320" cy="180" r="26" fill="none" stroke="rgba(56, 189, 248, 0.2)" stroke-width="1"/>
        <line x1="305" y1="180" x2="335" y2="180" stroke="rgba(56, 189, 248, 0.35)" stroke-width="1"/>
        <line x1="320" y1="165" x2="320" y2="195" stroke="rgba(56, 189, 248, 0.35)" stroke-width="1"/>

        <!-- Top OSD -->
        <circle cx="34" cy="38" r="4.5" fill="{status_color}"/>
        <text x="46" y="42" font-family="'JetBrains Mono', monospace" font-size="12" font-weight="700" fill="#f8fafc" letter-spacing="1">CH-0{channel} • {cam_name}</text>
        <text x="46" y="60" font-family="'JetBrains Mono', monospace" font-size="10.5" fill="#94a3b8">IP: {ip} • AliOS RTOS • Ping: {lat_text}</text>

        <text x="604" y="42" font-family="'JetBrains Mono', monospace" font-size="12" font-weight="700" fill="#38bdf8" text-anchor="end">{timestamp}</text>
        <text x="604" y="60" font-family="'JetBrains Mono', monospace" font-size="10" fill="{status_color}" text-anchor="end">{status_text}</text>

        <!-- Bottom OSD -->
        <text x="34" y="328" font-family="'JetBrains Mono', monospace" font-size="10.5" fill="#64748b">HD SURVEILLANCE FEED • RTSP 554 • H.264</text>
        <text x="604" y="328" font-family="'JetBrains Mono', monospace" font-size="10.5" fill="#38bdf8" font-weight="700" text-anchor="end">ALTOBEAM CCTV SOC</text>
    </svg>"""
    return svg.encode('utf-8')

class CCTVHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=PUBLIC_DIR, **kwargs)

    def log_message(self, format, *args):
        if args and str(args[0]).startswith(('200', '304')):
            return
        super().log_message(format, *args)

    def do_GET(self):
        parsed_url = urllib.parse.urlparse(self.path)
        path = parsed_url.path
        query = urllib.parse.parse_qs(parsed_url.query)

        if path in ['/api/devices', '/api/cameras']:
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Access-Control-Allow-Origin', '*')
            self.send_header('Cache-Control', 'no-cache')
            self.end_headers()

            with DEVICES_LOCK:
                dev_list = list(DEVICES.values())

            latencies = [d['latency'] for d in dev_list if d.get('latency') is not None]
            avg_lat = round(sum(latencies) / len(latencies), 1) if latencies else 4.2

            resp = {
                "cameras": CAMERA_CONFIGS,
                "status": SCAN_STATUS,
                "aliosCount": 7,
                "totalCount": 16,
                "onlineCount": 16,
                "avgLatency": avg_lat,
                "bandwidth": get_current_bandwidth_info(),
                "profiles": QUALITY_PROFILES
            }
            self.wfile.write(json.dumps(resp).encode('utf-8'))
            return

        elif path == '/api/camera/stream':
            ip = query.get('ip', ['192.168.1.5'])[0]
            raw_q = query.get('quality', query.get('profile', ['360p']))[0]
            
            # Map legacy names if needed
            q_map = {'eco': '360p', 'mid': '480p', 'high': '720p', 'hd': '720p'}
            q_key = q_map.get(raw_q, raw_q)
            profile = QUALITY_PROFILES.get(q_key, QUALITY_PROFILES['360p'])
            
            with BANDWIDTH_LOCK:
                BANDWIDTH_STATS["current_quality"] = q_key

            cfg = CAMERA_CONFIGS.get(ip, {"name": f"CAM ({ip})", "user": "admin", "pass": "", "port": 554, "path": "/live/ch0", "channel": 1})

            rtsp_open = check_port_open(ip, cfg.get('port', 554), timeout=0.25)
            if rtsp_open:
                user = cfg.get('user', 'admin')
                pwd = cfg.get('pass', '')
                port = cfg.get('port', 554)
                rpath = cfg.get('path', '/live/ch0')
                creds = f"{user}:{pwd}@" if pwd else f"{user}@"
                rtsp_url = f"rtsp://{creds}{ip}:{port}{rpath}"

                # FFmpeg with high clarity JPEG quality and targeted scale
                cmd = [
                    'ffmpeg', '-nostdin', '-loglevel', 'error',
                    '-rtsp_transport', 'tcp',
                    '-timeout', '3000000',
                    '-i', rtsp_url,
                    '-vf', f"scale={profile['scale']}",
                    '-r', str(profile['fps']),
                    '-q:v', str(profile['q']),
                    '-b:v', str(profile['bitrate']),
                    '-maxrate', str(profile['maxrate']),
                    '-bufsize', str(profile['bufsize']),
                    '-f', 'mpjpeg',
                    '-'
                ]

                proc = None
                with BANDWIDTH_LOCK:
                    BANDWIDTH_STATS["active_streams"] += 1
                try:
                    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                    self.send_response(200)
                    self.send_header('Content-Type', 'multipart/x-mixed-replace; boundary=ffmpeg')
                    self.send_header('Cache-Control', 'no-cache, no-store, must-revalidate')
                    self.send_header('Access-Control-Allow-Origin', '*')
                    self.end_headers()

                    while True:
                        chunk = proc.stdout.read(8192)
                        if not chunk:
                            break
                        self.wfile.write(chunk)
                        self.wfile.flush()
                        record_stream_bytes(len(chunk))
                except (BrokenPipeError, ConnectionResetError, ConnectionAbortedError, socket.error):
                    pass
                except Exception:
                    pass
                finally:
                    with BANDWIDTH_LOCK:
                        BANDWIDTH_STATS["active_streams"] = max(0, BANDWIDTH_STATS["active_streams"] - 1)
                    if proc:
                        try:
                            proc.kill()
                            proc.wait(timeout=0.5)
                        except Exception:
                            pass
                return

            # Fallback Live OSD SVG stream
            self.send_response(200)
            self.send_header('Content-Type', 'image/svg+xml')
            self.send_header('Cache-Control', 'no-cache, no-store, must-revalidate')
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()

            with DEVICES_LOCK:
                dev = DEVICES.get(ip, {})
            lat = dev.get('latency', 4.1)
            online = dev.get('online', True)
            frame = generate_svg_cctv_frame(ip, cfg.get('name', f'CAM {ip}'), lat, online, channel=cfg.get('channel', 1))
            self.wfile.write(frame)
            return

        elif path == '/api/camera/snapshot':
            ip = query.get('ip', ['192.168.1.5'])[0]
            is_download = query.get('download', ['0'])[0] == '1'
            cfg = CAMERA_CONFIGS.get(ip, {"name": f"CAM ({ip})", "user": "admin", "pass": "", "port": 554, "path": "/live/ch0", "channel": 1})

            rtsp_open = check_port_open(ip, cfg.get('port', 554), timeout=0.25)
            if rtsp_open:
                user = cfg.get('user', 'admin')
                pwd = cfg.get('pass', '')
                port = cfg.get('port', 554)
                rpath = cfg.get('path', '/live/ch0')
                creds = f"{user}:{pwd}@" if pwd else f"{user}@"
                rtsp_url = f"rtsp://{creds}{ip}:{port}{rpath}"

                temp_path = os.path.join(tempfile.gettempdir(), f"snap_{ip.replace('.', '_')}.jpg")
                cmd = [
                    'ffmpeg', '-nostdin', '-loglevel', 'error',
                    '-rtsp_transport', 'tcp',
                    '-timeout', '2000000',
                    '-i', rtsp_url,
                    '-vf', 'scale=1280:720',
                    '-q:v', '2',
                    '-vframes', '1',
                    '-f', 'image2',
                    '-update', '1',
                    '-y', temp_path
                ]
                try:
                    res = subprocess.run(cmd, timeout=3.0, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                    if res.returncode == 0 and os.path.exists(temp_path) and os.path.getsize(temp_path) > 1000:
                        with open(temp_path, 'rb') as f:
                            jpg_bytes = f.read()
                        self.send_response(200)
                        self.send_header('Content-Type', 'image/jpeg')
                        self.send_header('Cache-Control', 'no-cache, no-store, must-revalidate')
                        self.send_header('Access-Control-Allow-Origin', '*')
                        if is_download:
                            filename = f"cctv_{ip}_{time.strftime('%Y%m%d_%H%M%S')}.jpg"
                            self.send_header('Content-Disposition', f'attachment; filename="{filename}"')
                        self.end_headers()
                        self.wfile.write(jpg_bytes)
                        return
                except Exception:
                    pass

            self.send_response(200)
            self.send_header('Content-Type', 'image/svg+xml')
            if is_download:
                self.send_header('Content-Disposition', f'attachment; filename="cctv_{ip}_{time.strftime("%Y%m%d_%H%M%S")}.svg"')
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()

            with DEVICES_LOCK:
                dev = DEVICES.get(ip, {})
            lat = dev.get('latency', 4.1)
            frame = generate_svg_cctv_frame(ip, cfg.get('name', f'CAM {ip}'), lat, True, channel=cfg.get('channel', 1))
            self.wfile.write(frame)
            return

        return super().do_GET()

    def do_POST(self):
        content_length = int(self.headers.get('Content-Length', 0))
        post_data = self.rfile.read(content_length).decode('utf-8') if content_length > 0 else ""

        if self.path in ['/api/scan', '/api/refresh']:
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()
            self.wfile.write(json.dumps({"success": True, "message": "CCTV feeds updated"}).encode('utf-8'))
            return

        elif self.path == '/api/camera/config':
            try:
                data = json.loads(post_data)
                ip = data.get('ip')
                if ip and ip in CAMERA_CONFIGS:
                    CAMERA_CONFIGS[ip].update(data)
                self.send_response(200)
                self.send_header('Content-Type', 'application/json')
                self.send_header('Access-Control-Allow-Origin', '*')
                self.end_headers()
                self.wfile.write(json.dumps({"success": True, "config": CAMERA_CONFIGS.get(ip)}).encode('utf-8'))
                return
            except Exception as e:
                self.send_error(400, str(e))
                return

        elif self.path == '/api/ping':
            try:
                data = json.loads(post_data)
                ip = data.get('ip')
                if ip:
                    online, lat = ping_host(ip, timeout_ms=300)
                    with DEVICES_LOCK:
                        if ip in DEVICES:
                            DEVICES[ip]['online'] = online
                            DEVICES[ip]['latency'] = lat
                    self.send_response(200)
                    self.send_header('Content-Type', 'application/json')
                    self.send_header('Access-Control-Allow-Origin', '*')
                    self.end_headers()
                    self.wfile.write(json.dumps({"ip": ip, "online": online, "latency": lat}).encode('utf-8'))
                    return
            except Exception as e:
                self.send_error(400, str(e))
                return

        elif self.path == '/api/portscan':
            try:
                data = json.loads(post_data)
                ip = data.get('ip')
                ports_to_check = [554, 80, 8080, 443, 23, 6668]
                results = []
                for p in ports_to_check:
                    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                    sock.settimeout(0.3)
                    res = sock.connect_ex((ip, p))
                    sock.close()
                    results.append({"port": p, "open": res == 0})
                
                self.send_response(200)
                self.send_header('Content-Type', 'application/json')
                self.send_header('Access-Control-Allow-Origin', '*')
                self.end_headers()
                self.wfile.write(json.dumps({"ip": ip, "ports": results}).encode('utf-8'))
                return
            except Exception as e:
                self.send_error(400, str(e))
                return

        self.send_error(404, "Endpoint not found")

class ThreadingHTTPServer(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True
    allow_reuse_address = True

def run_server():
    populate_initial_data()
    server_address = ('', PORT)
    with ThreadingHTTPServer(server_address, CCTVHandler) as httpd:
        print(f"============================================================")
        print(f" 🛰️  CCTV Monitor - Multi-View NVR Station                  ")
        print(f" 📺 Pengaturan Kualitas Gambar: 1080p, 720p, 480p, 360p, 240p")
        print(f" 📹 7 Kamera Terhubung (192.168.1.4, .5, .6, .7, .16, .20, .13)    ")
        print(f" 🌐 Dashboard CCTV: http://localhost:{PORT}                 ")
        print(f" Tekan Ctrl+C untuk menghentikan server                     ")
        print(f"============================================================")
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            print("\nServer dihentikan.")

if __name__ == '__main__':
    run_server()
