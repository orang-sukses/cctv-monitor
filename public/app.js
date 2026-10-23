/**
 * CCTV Monitor - Multi-View NVR Station
 * Frontend Controller • True 3x3 Surveillance Matrix & YouTube-Style Quality Settings
 */

const CAMERA_IPS = [
    '192.168.1.4',
    '192.168.1.5',
    '192.168.1.6',
    '192.168.1.7',
    '192.168.1.16',
    '192.168.1.20',
    '192.168.1.13'
];

const STATE = {
    quality: '360p',      // Default: 360p (Hemat Kuota & Ringan)
    currentLayout: '3x3',  // '3x3' | 'quad' | 'solo'
    soloIp: null,
    cameras: {
        '192.168.1.4': { name: 'Ruang Utama', user: 'admin', pass: '', port: 554, path: '/live/ch0', channel: 1 },
        '192.168.1.5': { name: 'Pintu Masuk', user: 'admin', pass: '', port: 554, path: '/live/ch0', channel: 2 },
        '192.168.1.6': { name: 'Halaman Depan', user: 'admin', pass: '', port: 554, path: '/live/ch0', channel: 3 },
        '192.168.1.7': { name: 'Koridor Samping', user: 'admin', pass: '', port: 554, path: '/live/ch0', channel: 4 },
        '192.168.1.16': { name: 'Kamera 16', user: 'admin', pass: '', port: 554, path: '/live/ch0', channel: 5 },
        '192.168.1.20': { name: 'Kamera 20', user: 'admin', pass: '', port: 554, path: '/live/ch0', channel: 6 },
        '192.168.1.13': { name: 'Kamera 13', user: 'admin', pass: '', port: 554, path: '/live/ch0', channel: 7 },
    },
    currentCamConfigIp: null
};

// Toast notification helper
function showToast(message, type = 'info') {
    const container = document.getElementById('toastContainer');
    if (!container) return;
    const toast = document.createElement('div');
    toast.className = 'toast';
    const icon = type === 'success' ? '✅' : type === 'error' ? '❌' : 'ℹ️';
    toast.innerHTML = `<span>${icon}</span> <span>${message}</span>`;
    container.appendChild(toast);
    setTimeout(() => {
        toast.style.opacity = '0';
        toast.style.transform = 'translateY(10px)';
        setTimeout(() => toast.remove(), 250);
    }, 3000);
}

function copyToClipboard(text, label = 'Data') {
    navigator.clipboard.writeText(text).then(() => {
        showToast(`${label} disalin ke clipboard!`, 'success');
    }).catch(() => {
        showToast(`Gagal menyalin: ${text}`, 'error');
    });
}

function copyRtspUrl(ip) {
    const cfg = STATE.cameras[ip] || { user: 'admin', pass: '', port: 554, path: '/live/ch0' };
    const creds = cfg.pass ? `${cfg.user}:${cfg.pass}@` : `${cfg.user}@`;
    const url = `rtsp://${creds}${ip}:${cfg.port}${cfg.path}`;
    copyToClipboard(url, `URL RTSP ${cfg.name || ip}`);
}

// Download HD Snapshot from Camera
function downloadSnapshot(ip) {
    const a = document.createElement('a');
    a.href = `/api/camera/snapshot?ip=${ip}&download=1&t=${Date.now()}`;
    a.target = '_blank';
    document.body.appendChild(a);
    a.click();
    a.remove();
    showToast(`Snapshot ${STATE.cameras[ip]?.name || ip} diunduh!`, 'success');
}

// Download snapshot of all cameras
function downloadSnapshotAll() {
    showToast('Mengunduh snapshot 7 kamera...', 'info');
    CAMERA_IPS.forEach((ip, idx) => {
        setTimeout(() => {
            downloadSnapshot(ip);
        }, idx * 400);
    });
}

// Initialize / Update stream URLs with current YouTube-style quality setting
function applyStreamUrls(forceReload = false) {
    const q = STATE.quality;
    const now = Date.now();
    const badges = {
        '1080p': '1080p FHD',
        '720p': '720p HD',
        '480p': '480p SD',
        '360p': '360p',
        '240p': '240p'
    };

    const headerBadge = document.getElementById('qualityHeaderBadge');
    if (headerBadge) headerBadge.textContent = `${q.toUpperCase()} ACTIVE`;

    CAMERA_IPS.forEach((ip, idx) => {
        const img = document.getElementById(`stream-${ip}`);
        const badge = document.getElementById(`badge-${ip}`);
        if (badge) badge.textContent = badges[q] || `${q.toUpperCase()} HD`;

        if (img) {
            setTimeout(() => {
                const streamUrl = `/api/camera/stream?ip=${ip}&quality=${q}${forceReload ? `&t=${now}` : ''}`;
                img.src = streamUrl;
            }, idx * 120);
        }
    });
}

// Update running digital surveillance clock in header
function updateClocks() {
    const now = new Date();
    const timeStr = now.toLocaleTimeString('id-ID', { hour12: false });
    const mainClock = document.getElementById('nvrMainClock');
    if (mainClock) mainClock.textContent = timeStr;
}

// Fetch stats and live bandwidth speed from server
async function fetchSystemStats() {
    try {
        const res = await fetch('/api/cameras');
        if (!res.ok) return;
        const data = await res.json();

        if (data.cameras) {
            STATE.cameras = data.cameras;
            CAMERA_IPS.forEach(ip => {
                const nameEl = document.getElementById(`name-${ip}`);
                if (nameEl && data.cameras[ip]) {
                    nameEl.textContent = data.cameras[ip].name || `CAM ${ip}`;
                }
            });
        }

        // Update the HUD Metric Cards
        const countAlios = document.getElementById('countAlios');
        const countTotal = document.getElementById('countTotal');
        const countOnline = document.getElementById('countOnline');
        const avgLat = document.getElementById('avgLatency');

        if (countAlios) countAlios.textContent = data.aliosCount || 7;
        if (countTotal) countTotal.textContent = data.totalCount || 16;
        if (countOnline) countOnline.textContent = data.onlineCount || 16;
        if (avgLat) avgLat.innerHTML = `${data.avgLatency || 4.2} <span class="unit">ms</span>`;

        // Update Real-Time Speed Internet CCTV Display
        const speedVal = document.getElementById('totalBandwidthSpeed');
        const speedSub = document.getElementById('speedSubtitle');
        if (data.bandwidth) {
            const bw = data.bandwidth;
            if (speedVal) {
                speedVal.innerHTML = `${bw.mbps} <span class="unit">Mbps</span>`;
            }
            if (speedSub) {
                speedSub.textContent = `7 Kamera (${bw.kBps} KB/s) • Kualitas ${STATE.quality.toUpperCase()}`;
            }
        }
    } catch (e) {
        // Silently retain cached UI on network jitter
    }
}

// Layout Switching (3x3 Matrix, 2x2 Quad, or Solo)
function setLayout(layoutType, focusIp = null) {
    const matrix = document.getElementById('cctvMatrix');
    if (!matrix) return;

    STATE.currentLayout = layoutType;
    matrix.className = `cctv-matrix layout-${layoutType}`;

    // Reset layout buttons
    document.querySelectorAll('.layout-btn').forEach(btn => btn.classList.remove('active'));
    if (layoutType === '3x3') document.getElementById('btnLayout3x3')?.classList.add('active');
    if (layoutType === 'quad') document.getElementById('btnLayoutQuad')?.classList.add('active');

    // Reset box classes
    CAMERA_IPS.forEach(ip => {
        const box = document.getElementById(`box-${ip}`);
        if (box) {
            box.classList.remove('solo-active');
        }
    });

    if (layoutType === 'solo' && focusIp) {
        STATE.soloIp = focusIp;
        const targetBox = document.getElementById(`box-${focusIp}`);
        if (targetBox) targetBox.classList.add('solo-active');
    }

    updateChannelTabs(focusIp);
}

function toggleSoloCamera(ip) {
    if (STATE.currentLayout === 'solo' && STATE.soloIp === ip) {
        // Return to 3x3 Grid
        setLayout('3x3');
        showToast('Kembali ke Tampilan 3x3 Grid', 'info');
    } else {
        // Maximize this camera to full monitor
        setLayout('solo', ip);
        showToast(`Layar Penuh: ${STATE.cameras[ip]?.name || ip} (${STATE.quality.toUpperCase()})`, 'info');
    }
}

function switchChannelView(target) {
    if (target === 'all') {
        setLayout('3x3');
    } else {
        toggleSoloCamera(target);
    }
}

function updateChannelTabs(activeIp) {
    const tabs = document.querySelectorAll('.ch-tab-btn');
    tabs.forEach(tab => tab.classList.remove('active'));

    if (!activeIp || STATE.currentLayout === '3x3') {
        tabs[0]?.classList.add('active');
    } else {
        CAMERA_IPS.forEach((ip, idx) => {
            if (ip === activeIp) {
                tabs[idx + 1]?.classList.add('active');
            }
        });
    }
}

// Camera Configuration Modal
function openCamConfig(ip) {
    STATE.currentCamConfigIp = ip;
    const cfg = STATE.cameras[ip] || { name: `CAM ${ip}`, user: 'admin', pass: '', port: 554, path: '/live/ch0' };
    document.getElementById('cfgCamIp').textContent = ip;
    document.getElementById('cfgCamName').value = cfg.name || `CAM ${ip}`;
    document.getElementById('cfgCamUser').value = cfg.user || 'admin';
    document.getElementById('cfgCamPass').value = cfg.pass || '';
    document.getElementById('cfgCamPort').value = cfg.port || 554;
    document.getElementById('cfgCamPath').value = cfg.path || '/live/ch0';
    document.getElementById('camConfigModal').classList.add('active');
}

async function saveCamConfig() {
    const ip = STATE.currentCamConfigIp;
    if (!ip) return;
    const cfg = {
        ip,
        name: document.getElementById('cfgCamName').value.trim() || `CAM ${ip}`,
        user: document.getElementById('cfgCamUser').value.trim() || 'admin',
        pass: document.getElementById('cfgCamPass').value.trim(),
        port: parseInt(document.getElementById('cfgCamPort').value, 10) || 554,
        path: document.getElementById('cfgCamPath').value.trim() || '/live/ch0'
    };

    try {
        const res = await fetch('/api/camera/config', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(cfg)
        });
        const data = await res.json();
        STATE.cameras[ip] = data.config;

        const nameEl = document.getElementById(`name-${ip}`);
        if (nameEl) nameEl.textContent = cfg.name;

        document.getElementById('camConfigModal').classList.remove('active');
        showToast(`Pengaturan kamera ${cfg.name} disimpan!`, 'success');
        applyStreamUrls(true);
    } catch (e) {
        showToast('Gagal menyimpan pengaturan', 'error');
    }
}

// Event Listeners Setup
function setupEventListeners() {
    // YouTube-Style Video Quality Selection (1080p, 720p, 480p, 360p, 240p)
    const qualitySelect = document.getElementById('qualitySelect');
    if (qualitySelect) {
        qualitySelect.addEventListener('change', (e) => {
            STATE.quality = e.target.value;
            applyStreamUrls(true);
            showToast(`Kualitas Video diubah ke: ${STATE.quality.toUpperCase()}!`, 'success');
        });
    }

    // Layout buttons
    document.getElementById('btnLayout3x3')?.addEventListener('click', () => setLayout('3x3'));
    document.getElementById('btnLayoutQuad')?.addEventListener('click', () => setLayout('quad'));

    // Reconnect / Refresh streams
    document.getElementById('btnReconnectStreams')?.addEventListener('click', () => {
        applyStreamUrls(true);
        showToast('Menghubungkan ulang semua feed...', 'info');
    });

    // Snapshot All
    document.getElementById('btnSnapshotAll')?.addEventListener('click', downloadSnapshotAll);

    // Fullscreen Toggle
    document.getElementById('btnFullscreen')?.addEventListener('click', () => {
        if (!document.fullscreenElement) {
            document.documentElement.requestFullscreen().catch(() => {});
            document.getElementById('fullscreenIcon').textContent = '✕';
        } else {
            document.exitFullscreen().catch(() => {});
            document.getElementById('fullscreenIcon').textContent = '⛶';
        }
    });

    // Header Handle Toggle click (optional lock/unlock)
    const headerWrapper = document.getElementById('nvrHeaderWrapper');
    document.getElementById('nvrHeaderHandle')?.addEventListener('click', (e) => {
        headerWrapper.classList.toggle('is-open');
    });

    // VLC Guide Modal
    const vlcModal = document.getElementById('vlcModal');
    document.getElementById('btnOpenVLCGuide')?.addEventListener('click', () => vlcModal.classList.add('active'));
    document.getElementById('btnCloseVlcModal')?.addEventListener('click', () => vlcModal.classList.remove('active'));
    document.getElementById('btnCloseVlcBtn')?.addEventListener('click', () => vlcModal.classList.remove('active'));

    // Cam Config Modal
    const camModal = document.getElementById('camConfigModal');
    document.getElementById('btnCloseCamConfigModal')?.addEventListener('click', () => camModal.classList.remove('active'));
    document.getElementById('btnCancelCamConfig')?.addEventListener('click', () => camModal.classList.remove('active'));
    document.getElementById('btnSaveCamConfig')?.addEventListener('click', saveCamConfig);

    // Keyboard Shortcuts: 1-7 for solo camera, 0 for 3x3, F for fullscreen
    document.addEventListener('keydown', (e) => {
        if (e.target.tagName === 'INPUT' || e.target.tagName === 'SELECT') return;
        if (e.key >= '1' && e.key <= '7') {
            const idx = parseInt(e.key, 10) - 1;
            if (CAMERA_IPS[idx]) toggleSoloCamera(CAMERA_IPS[idx]);
        } else if (e.key === '0' || e.key === 'Escape') {
            setLayout('3x3');
        } else if (e.key.toLowerCase() === 'f') {
            document.getElementById('btnFullscreen')?.click();
        }
    });
}

// Startup
document.addEventListener('DOMContentLoaded', () => {
    setupEventListeners();
    updateClocks();
    setInterval(updateClocks, 1000);
    applyStreamUrls();
    fetchSystemStats();
    setInterval(fetchSystemStats, 2500); // Poll bandwidth every 2.5 seconds for live speedometer
});
