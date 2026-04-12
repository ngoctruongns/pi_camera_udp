#!/usr/bin/env python3

import rclpy
from rclpy.node import Node
from sensor_msgs.msg import Image
import cv2
from cv_bridge import CvBridge


class CameraBridge(Node):
    def __init__(self):
        super().__init__('camera_bridge')
        self.publisher_ = self.create_publisher(Image, 'camera/image_raw', 10)
        self.bridge = CvBridge()

        port = self.declare_parameter('port', 5000).get_parameter_value().integer_value

        # Raw H.264 over UDP — matches libcamera-vid --codec h264 output.
        # avdec_h264 decodes in software; appsink is configured for minimal
        # latency (drop=true, max-buffers=1).
        pipeline = (
            f'udpsrc port={port} '
            '! h264parse '
            '! avdec_h264 '
            '! videoconvert '
            '! video/x-raw,format=BGR '
            '! appsink drop=true max-buffers=1 emit-signals=true sync=false'
        )

        self.cap = cv2.VideoCapture(pipeline, cv2.CAP_GSTREAMER)

        if not self.cap.isOpened():
            self.get_logger().error(
                f'Cannot open GStreamer pipeline on port {port}. '
                'Make sure Pi is streaming and GStreamer is built with '
                'avdec_h264 support (gstreamer1.0-libav).'
            )
            return

        self.get_logger().info(f'Receiving raw H.264 stream on UDP port {port}')
        self.timer = self.create_timer(1.0 / 30.0, self.timer_callback)

    def timer_callback(self):
        ret, frame = self.cap.read()
        if ret:
            msg = self.bridge.cv2_to_imgmsg(frame, encoding='bgr8')
            msg.header.stamp = self.get_clock().now().to_msg()
            msg.header.frame_id = 'camera_link'
            self.publisher_.publish(msg)

    def destroy_node(self):
        if hasattr(self, 'cap') and self.cap.isOpened():
            self.cap.release()
        super().destroy_node()


def main():
    rclpy.init()
    node = CameraBridge()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()