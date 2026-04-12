#include <atomic>
#include <string>
#include <thread>

#include <gst/app/gstappsink.h>
#include <gst/gst.h>

#include <opencv2/opencv.hpp>

#include <cv_bridge/cv_bridge.h>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <std_msgs/msg/header.hpp>

class CameraBridgeNode : public rclcpp::Node
{
public:
  CameraBridgeNode()
  : Node("camera_bridge")
  {
    int port = static_cast<int>(declare_parameter<int>("port", 5000));

    publisher_ = create_publisher<sensor_msgs::msg::Image>("camera/image_raw", 10);

    gst_init(nullptr, nullptr);

    // Raw H.264 over UDP — matches libcamera-vid --codec h264 output.
    // appsink: drop=true + max-buffers=1 ensure we always get the latest frame.
    const std::string pipeline_str =
      "udpsrc port=" + std::to_string(port) +
      " ! h264parse"
      " ! avdec_h264"
      " ! videoconvert"
      " ! video/x-raw,format=BGR"
      " ! appsink name=appsink drop=true max-buffers=1 sync=false";

    GError * error = nullptr;
    pipeline_ = gst_parse_launch(pipeline_str.c_str(), &error);
    if (error) {
      RCLCPP_ERROR(get_logger(), "Pipeline parse error: %s", error->message);
      g_error_free(error);
      return;
    }

    appsink_ = gst_bin_get_by_name(GST_BIN(pipeline_), "appsink");
    if (!appsink_) {
      RCLCPP_ERROR(get_logger(), "Cannot get appsink element from pipeline");
      return;
    }

    GstStateChangeReturn ret = gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
      RCLCPP_ERROR(get_logger(), "Failed to set pipeline to PLAYING");
      return;
    }

    RCLCPP_INFO(get_logger(), "Receiving raw H.264 stream on UDP port %d", port);
    running_ = true;
    capture_thread_ = std::thread(&CameraBridgeNode::captureLoop, this);
  }

  ~CameraBridgeNode() override
  {
    running_ = false;
    if (capture_thread_.joinable()) {
      capture_thread_.join();
    }
    if (appsink_) {
      gst_object_unref(appsink_);
      appsink_ = nullptr;
    }
    if (pipeline_) {
      gst_element_set_state(pipeline_, GST_STATE_NULL);
      gst_object_unref(pipeline_);
      pipeline_ = nullptr;
    }
  }

private:
  void captureLoop()
  {
    while (running_) {
      // Block up to 100 ms waiting for a sample; avoids busy-spin
      GstSample * sample =
        gst_app_sink_try_pull_sample(GST_APP_SINK(appsink_), 100 * GST_MSECOND);

      if (!sample) {
        continue;
      }

      GstCaps * caps = gst_sample_get_caps(sample);
      GstStructure * s = gst_caps_get_structure(caps, 0);

      gint width = 0, height = 0;
      gst_structure_get_int(s, "width", &width);
      gst_structure_get_int(s, "height", &height);

      GstBuffer * buffer = gst_sample_get_buffer(sample);
      GstMapInfo map;

      if (width > 0 && height > 0 && gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        cv::Mat frame(height, width, CV_8UC3, map.data);

        std_msgs::msg::Header header;
        header.stamp = now();
        header.frame_id = "camera_link";

        auto msg = cv_bridge::CvImage(header, "bgr8", frame).toImageMsg();
        publisher_->publish(*msg);

        gst_buffer_unmap(buffer, &map);
      }

      gst_sample_unref(sample);
    }
  }

  rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr publisher_;
  GstElement * pipeline_{nullptr};
  GstElement * appsink_{nullptr};
  std::thread capture_thread_;
  std::atomic<bool> running_{false};
};

int main(int argc, char * argv[])
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<CameraBridgeNode>();
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
