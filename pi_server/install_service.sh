#!/usr/bin/env bash
# install_service.sh — Install pi-camera-stream.service on Raspberry Pi
#
# Usage (run on the Pi):
#   ./install_service.sh <laptop_ip> [port] [profile]
#
# Example:
#   ./install_service.sh 192.168.10.37
#   ./install_service.sh 192.168.10.37 5000 high

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SERVICE_NAME="pi-camera-stream"
SERVICE_FILE="/etc/systemd/system/${SERVICE_NAME}.service"

LAPTOP_IP="${1:-}"
PORT="${2:-5000}"
PROFILE="${3:-balanced}"

if [[ -z "${LAPTOP_IP}" ]]; then
  echo "Usage: $0 <laptop_ip> [port] [profile]"
  echo "Example: $0 192.168.10.37 5000 balanced"
  exit 1
fi

# Resolve absolute path to run_camera_stream.sh
STREAM_SCRIPT="${SCRIPT_DIR}/run_camera_stream.sh"
if [[ ! -x "${STREAM_SCRIPT}" ]]; then
  echo "Error: ${STREAM_SCRIPT} not found or not executable."
  exit 1
fi

# Detect current user (do not run service as root)
SERVICE_USER="${SUDO_USER:-${USER}}"
if [[ "${SERVICE_USER}" == "root" ]]; then
  echo "Warning: running service as root is not recommended."
fi

echo "Installing ${SERVICE_NAME}.service..."
echo "  Stream script : ${STREAM_SCRIPT}"
echo "  Run as user   : ${SERVICE_USER}"
echo "  Laptop IP     : ${LAPTOP_IP}"
echo "  Port          : ${PORT}"
echo "  Profile       : ${PROFILE}"

sudo tee "${SERVICE_FILE}" > /dev/null << EOF
[Unit]
Description=Pi Camera UDP Stream
After=network-online.target
Wants=network-online.target

[Service]
Type=simple
User=${SERVICE_USER}
Environment="LAPTOP_IP=${LAPTOP_IP}"
Environment="PORT=${PORT}"
Environment="PROFILE=${PROFILE}"
ExecStart=${STREAM_SCRIPT} \${LAPTOP_IP} \${PORT} \${PROFILE}
Restart=on-failure
RestartSec=3s
TimeoutStartSec=15s
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl enable "${SERVICE_NAME}.service"
sudo systemctl restart "${SERVICE_NAME}.service"

echo ""
echo "Done. Service installed and started."
echo ""
echo "Useful commands:"
echo "  sudo systemctl status  ${SERVICE_NAME}"
echo "  sudo systemctl stop    ${SERVICE_NAME}"
echo "  sudo systemctl start   ${SERVICE_NAME}"
echo "  sudo systemctl disable ${SERVICE_NAME}  # remove from autostart"
echo "  journalctl -u ${SERVICE_NAME} -f        # live logs"
