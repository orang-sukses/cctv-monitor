#!/usr/bin/env bash
# Ambil CCTVMonitor.exe terbaru dari branch latest-exe (hasil build GitHub Actions)
# lalu timpa ke folder Windows. Pakai: ./update-exe.sh   (tutup CCTV Monitor dulu)
set -e
DEST="${DEST:-/mnt/c/Users/ahmad/Downloads/CCTVMonitor-windows/CCTVMonitor-windows/CCTVMonitor}"
cd "$(dirname "$0")"
git fetch -q origin latest-exe
tmp=$(mktemp)
git show FETCH_HEAD:CCTVMonitor.exe > "$tmp"
cp "$tmp" "$DEST/CCTVMonitor.exe" || { echo "Gagal menimpa exe: tutup dulu CCTV Monitor."; rm -f "$tmp"; exit 1; }
rm -f "$tmp"
echo "OK: $DEST/CCTVMonitor.exe diperbarui ($(git log -1 --format=%s FETCH_HEAD))"
