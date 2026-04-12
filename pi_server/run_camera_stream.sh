#!/usr/bin/env bash

set -euo pipefail

# Usage:
#   ./run_camera_stream.sh <laptop_ip> [port] [profile]
#   ./run_camera_stream.sh <laptop_ip> [port] [width] [height] [fps] [bitrate]
# Example:
#   ./run_camera_stream.sh 192.168.1.10 5000 balanced
#   ./run_camera_stream.sh 192.168.1.10 5000 1280 720 30 2000000
#
# Profiles:
#   low      - 640x480  @ 20fps, 1.2 Mbps
#   balanced - 848x480  @ 20fps, 1.8 Mbps  (default)
#   high     - 1280x720 @ 25fps, 3.0 Mbps
#
# Protocol: raw H.264 over UDP (pkt_size=1316, inline SPS/PPS)
# Receive with: gst-launch-1.0 udpsrc port=<port> ! h264parse ! avdec_h264 ! ...

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <laptop_ip> [port] [profile]"
  echo "   or: $0 <laptop_ip> [port] [width] [height] [fps] [bitrate]"
  echo "Profiles: low | balanced | high"
  exit 1
fi

LAPTOP_IP="192.168.10.37"
PORT="${2:-5000}"
PROFILE="balanced"

if [[ $# -ge 3 ]]; then
  case "${3}" in
    low|balanced|high)
      PROFILE="${3}"
      ;;
    [0-9]*)
      PROFILE="manual"
      ;;
    *)
      echo "Error: unknown profile '${3}'. Use low | balanced | high."
      exit 1
      ;;
  esac
fi

if [[ "${PROFILE}" == "low" ]]; then
  WIDTH=640; HEIGHT=480; FPS=20; BITRATE=1200000
elif [[ "${PROFILE}" == "balanced" ]]; then
  WIDTH=848; HEIGHT=480; FPS=20; BITRATE=1800000
elif [[ "${PROFILE}" == "high" ]]; then
  WIDTH=1280; HEIGHT=720; FPS=25; BITRATE=3000000
else
  WIDTH="${3:-848}"
  HEIGHT="${4:-480}"
  FPS="${5:-20}"
  BITRATE="${6:-1800000}"
fi

if ! [[ "${PORT}" =~ ^[0-9]+$ && "${PORT}" -ge 1 && "${PORT}" -le 65535 ]]; then
  echo "Error: port must be an integer between 1 and 65535."
  exit 1
fi

if ! [[ "${WIDTH}" =~ ^[0-9]+$ && "${HEIGHT}" =~ ^[0-9]+$ && \
        "${FPS}" =~ ^[0-9]+$ && "${BITRATE}" =~ ^[0-9]+$ ]]; then
  echo "Error: width, height, fps, and bitrate must be positive integers."
  exit 1
fi

if ! command -v libcamera-vid >/dev/null 2>&1; then
  echo "Error: libcamera-vid is not installed."
  echo "Install it with: sudo apt install libcamera-apps"
  exit 1
fi

echo "Starting Pi camera stream..."
echo "Target  : ${LAPTOP_IP}:${PORT}"
echo "Profile : ${PROFILE}"
echo "Video   : ${WIDTH}x${HEIGHT} @ ${FPS}fps, bitrate=${BITRATE} bps"
echo "Protocol: raw H.264 over UDP"
echo ""
echo "Press Ctrl+C to stop."

# --inline   : embed SPS/PPS in every keyframe (required for mid-stream joins)
# --flush    : flush after each frame (low latency)
# --nopreview: headless (no display required on Pi)
# pkt_size=1316: fits in standard 1500-byte MTU ethernet frame with UDP/IP headers
exec libcamera-vid \
  --nopreview \
  -t 0 \
  --inline \
  --flush \
  --width "${WIDTH}" \
  --height "${HEIGHT}" \
  --framerate "${FPS}" \
  --bitrate "${BITRATE}" \
  --codec h264 \
  -o "udp://${LAPTOP_IP}:${PORT}?pkt_size=1316"
