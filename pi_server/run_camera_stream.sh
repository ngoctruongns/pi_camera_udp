#!/usr/bin/env bash

set -euo pipefail

# Usage:
#   ./run_camera_stream.sh <laptop_ip> [port] [profile]
#   ./run_camera_stream.sh <laptop_ip> [port] [width] [height] [fps] [bitrate]
# Example:
#   ./run_camera_stream.sh 192.168.1.10 5000 balanced
#   ./run_camera_stream.sh 192.168.1.10 5000 1280 720 30 2000000

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <laptop_ip> [port] [profile]"
  echo "   or: $0 <laptop_ip> [port] [width] [height] [fps] [bitrate]"
  echo "Profiles: low | balanced | high"
  exit 1
fi

LAPTOP_IP="$1"
PORT="${2:-5000}"
PROFILE="balanced"

if [[ $# -ge 3 ]]; then
  case "${3}" in
    low|balanced|high)
      PROFILE="${3}"
      ;;
    *)
      # Backward-compatible manual mode: width height fps bitrate
      PROFILE="manual"
      ;;
  esac
fi

if [[ "${PROFILE}" == "low" ]]; then
  WIDTH=640
  HEIGHT=480
  FPS=20
  BITRATE=1200000
elif [[ "${PROFILE}" == "balanced" ]]; then
  WIDTH=848
  HEIGHT=480
  FPS=20
  BITRATE=1800000
elif [[ "${PROFILE}" == "high" ]]; then
  WIDTH=1280
  HEIGHT=720
  FPS=25
  BITRATE=3000000
else
  WIDTH="${3:-848}"
  HEIGHT="${4:-480}"
  FPS="${5:-20}"
  BITRATE="${6:-1800000}"
fi

if ! [[ "${PORT}" =~ ^[0-9]+$ ]]; then
  echo "Error: port must be a positive integer."
  exit 1
fi

if ! [[ "${WIDTH}" =~ ^[0-9]+$ && "${HEIGHT}" =~ ^[0-9]+$ && "${FPS}" =~ ^[0-9]+$ && "${BITRATE}" =~ ^[0-9]+$ ]]; then
  echo "Error: width, height, fps, and bitrate must be positive integers."
  exit 1
fi

if ! command -v gst-launch-1.0 >/dev/null 2>&1; then
  echo "Error: gst-launch-1.0 is not installed."
  echo "Install GStreamer first on Raspberry Pi OS."
  exit 1
fi

if ! gst-inspect-1.0 libcamerasrc >/dev/null 2>&1; then
  echo "Error: libcamerasrc plugin not found."
  echo "Install package: gstreamer1.0-libcamera"
  exit 1
fi

if ! gst-inspect-1.0 v4l2h264enc >/dev/null 2>&1; then
  echo "Error: v4l2h264enc plugin not found."
  echo "This script expects Raspberry Pi hardware H264 encoder via v4l2h264enc."
  exit 1
fi

echo "Starting Pi camera RTP stream..."
echo "Target : ${LAPTOP_IP}:${PORT}"
echo "Mode   : ${PROFILE}"
echo "Video  : ${WIDTH}x${HEIGHT} @ ${FPS}fps, bitrate=${BITRATE}"

gst-launch-1.0 -e \
  libcamerasrc ! \
  video/x-raw,width=${WIDTH},height=${HEIGHT},framerate=${FPS}/1 ! \
  queue ! \
  v4l2h264enc extra-controls="controls,video_bitrate=${BITRATE}" ! \
  h264parse config-interval=-1 ! \
  rtph264pay pt=96 config-interval=1 ! \
  udpsink host=${LAPTOP_IP} port=${PORT} sync=false async=false
