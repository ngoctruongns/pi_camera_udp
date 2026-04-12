# Pi Camera UDP → ROS2 Bridge

Stream Raspberry Pi camera over raw H.264/UDP and publish as ROS2 image topics on a laptop.

## Architecture

```
Raspberry Pi                       Laptop (ROS2 Humble)
────────────────────────────────   ─────────────────────────────────────────
libcamera-vid                      ros2_pi_camera_bridge_node (C++)
  └─ raw H.264/UDP  ──────────►      └─ GStreamer (avdec_h264)
                                       └─ /camera/image_raw (sensor_msgs/Image)
                                                    │
                                             RViz2 Image display
```

## Requirements

### Raspberry Pi (sender)
- `libcamera-apps` (`libcamera-vid`)

```bash
sudo apt install libcamera-apps
```

### Laptop (receiver)

```bash
sudo apt install \
  ros-humble-cv-bridge \
  libopencv-dev \
  libgstreamer1.0-dev \
  libgstreamer-plugins-base1.0-dev \
  gstreamer1.0-plugins-good \
  gstreamer1.0-plugins-bad \
  gstreamer1.0-libav
```

## Build

```bash
cd ~/ros2_ws
colcon build --packages-select ros2_pi_camera_bridge
source install/setup.bash
```

## Usage

### 1 — Start stream on Raspberry Pi

```bash
cd ~/code_ws/pi_camera_udp/pi_server
./run_camera_stream.sh <laptop_ip>
```

**Profiles** (optional):

| Profile    | Resolution | FPS | Bitrate  |
|------------|-----------|-----|----------|
| `low`      | 640×480   | 20  | 1.2 Mbps |
| `balanced` | 848×480   | 20  | 1.8 Mbps |
| `high`     | 1280×720  | 25  | 3.0 Mbps |

```bash
./run_camera_stream.sh 192.168.10.37 5000 low
./run_camera_stream.sh 192.168.10.37 5000 balanced   # default
./run_camera_stream.sh 192.168.10.37 5000 high
```

**Manual** (width height fps bitrate):

```bash
./run_camera_stream.sh 192.168.10.37 5000 1280 720 30 3000000
```

### 2 — Autostart stream on Pi boot (optional)

Run once on the Pi to install a systemd service that starts the stream automatically on every boot:

```bash
cd ~/code_ws/pi_camera_udp/pi_server
./install_service.sh <laptop_ip> [port] [profile]
```

Example:

```bash
./install_service.sh 192.168.10.37              # balanced, port 5000
./install_service.sh 192.168.10.37 5000 high    # high profile
```

Running `install_service.sh` again with different arguments **overwrites** the previous config and restarts the service immediately — no manual cleanup needed.

Manage the service:

```bash
sudo systemctl status  pi-camera-stream    # check status
sudo systemctl stop    pi-camera-stream    # stop
sudo systemctl start   pi-camera-stream    # start
sudo systemctl disable pi-camera-stream    # remove from autostart
journalctl -u pi-camera-stream -f          # live logs
```

### 3 — Launch bridge + RViz on Laptop

```bash
source ~/ros2_ws/install/setup.bash
ros2 launch ros2_pi_camera_bridge camera_stream.launch.py
```

Override port if needed:

```bash
ros2 launch ros2_pi_camera_bridge camera_stream.launch.py port:=5001
```

### 3 — Verify

```bash
ros2 topic hz /camera/image_raw
```

## Notes

- Pi and laptop must be on the same network and use the same UDP port.
- Always pass the **laptop IP** (not Pi IP) to the sender script.
- If `avdec_h264` is missing, install `gstreamer1.0-libav`.
