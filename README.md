# Pi Camera UDP to ROS2 Bridge

Minimal setup to stream Raspberry Pi camera over RTP/UDP and publish ROS2 image topics on laptop.

## Architecture

- Pi (Raspberry Pi OS): `libcamerasrc` -> `v4l2h264enc` -> RTP/UDP
- Laptop (ROS2): RTP/UDP -> GStreamer decode -> `/camera/image_raw`, `/camera/camera_info`

## 1) Raspberry Pi OS (Sender)

Install dependencies:

```bash
sudo apt update
sudo apt install -y gstreamer1.0-tools \
  gstreamer1.0-plugins-base \
  gstreamer1.0-plugins-good \
  gstreamer1.0-plugins-bad \
  gstreamer1.0-libav \
  gstreamer1.0-libcamera
```

Run sender:

```bash
cd /home/tvn/ros2_ws/src/pi_camera_udp/pi_server
./run_camera_stream.sh <laptop_ip>
```

Profile mode:

```bash
./run_camera_stream.sh 192.168.1.10 5000 low
./run_camera_stream.sh 192.168.1.10 5000 balanced
./run_camera_stream.sh 192.168.1.10 5000 high
```

Manual mode:

```bash
./run_camera_stream.sh 192.168.1.10 5000 848 480 20 1800000
```

Recommended for diff-drive robot: `balanced`.

## 2) Laptop ROS2 Bridge

Install dependencies:

```bash
sudo apt update
sudo apt install -y ros-humble-cv-bridge libopencv-dev pkg-config \
  libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
  gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-libav
```

Build:

```bash
cd /home/tvn/ros2_ws
colcon build --packages-select ros2_pi_camera_bridge
```

Run:

```bash
source /home/tvn/ros2_ws/install/setup.bash
ros2 run ros2_pi_camera_bridge ros2_pi_camera_bridge_node --ros-args -p udp_port:=5000
```

## 3) Verify Topics

```bash
ros2 topic list | grep camera
ros2 topic hz /camera/image_raw
ros2 topic echo /camera/camera_info --once
```

Notes:

- Pi and laptop must use the same UDP port.
- Always pass laptop IP when running sender.
- If encoder plugin differs on your Pi image, edit pipeline in `pi_server/run_camera_stream.sh`.
