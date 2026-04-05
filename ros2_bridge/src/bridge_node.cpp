#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

#include <cv_bridge/cv_bridge.h>
#include <gst/app/gstappsink.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <opencv2/opencv.hpp>
#include <rclcpp/rclcpp.hpp>
#include <sensor_msgs/msg/camera_info.hpp>
#include <sensor_msgs/msg/image.hpp>
#include <std_msgs/msg/header.hpp>

class PiCameraBridgeNode : public rclcpp::Node
{
public:
    PiCameraBridgeNode()
    : Node("pi_camera_udp_bridge")
    {
        gst_init(nullptr, nullptr);

        const int udp_port = declare_parameter<int>("udp_port", 5000);
        frame_id_ = declare_parameter<std::string>("frame_id", "pi_camera_optical_frame");
        receiver_pipeline_ = declare_parameter<std::string>(
            "receiver_pipeline",
            default_pipeline(udp_port));

        image_pub_ = create_publisher<sensor_msgs::msg::Image>("camera/image_raw", 10);
        camera_info_pub_ = create_publisher<sensor_msgs::msg::CameraInfo>("camera/camera_info", 10);

        if (!start_pipeline())
        {
            throw std::runtime_error("Failed to initialize GStreamer receiver pipeline");
        }

        running_.store(true);
        worker_ = std::thread([this]() { this->bridge_loop(); });
        RCLCPP_INFO(get_logger(), "RTP/UDP -> ROS2 bridge started on UDP port %d", udp_port);
    }

    ~PiCameraBridgeNode() override
    {
        running_.store(false);
        if (worker_.joinable())
        {
            worker_.join();
        }

        stop_and_cleanup_pipeline();
    }

private:
    void stop_and_cleanup_pipeline()
    {
        if (pipeline_ != nullptr)
        {
            gst_element_set_state(pipeline_, GST_STATE_NULL);
        }

        if (appsink_ != nullptr)
        {
            gst_object_unref(appsink_);
            appsink_ = nullptr;
        }

        if (bus_ != nullptr)
        {
            gst_object_unref(bus_);
            bus_ = nullptr;
        }

        if (pipeline_ != nullptr)
        {
            gst_object_unref(pipeline_);
            pipeline_ = nullptr;
        }
    }

    static std::string default_pipeline(int udp_port)
    {
        return "udpsrc port=" + std::to_string(udp_port) +
               " caps=\"application/x-rtp, media=video, encoding-name=H264, payload=96, clock-rate=90000\" ! "
               "rtph264depay ! avdec_h264 ! videoconvert ! video/x-raw,format=BGR ! "
               "appsink name=video_sink sync=false max-buffers=1 drop=true";
    }

    bool start_pipeline()
    {
        GError* error = nullptr;
        pipeline_ = gst_parse_launch(receiver_pipeline_.c_str(), &error);
        if (pipeline_ == nullptr)
        {
            if (error != nullptr)
            {
                RCLCPP_ERROR(get_logger(), "Failed to parse receiver pipeline: %s", error->message);
                g_error_free(error);
            }
            return false;
        }

        appsink_ = GST_APP_SINK(gst_bin_get_by_name(GST_BIN(pipeline_), "video_sink"));
        if (appsink_ == nullptr)
        {
            RCLCPP_ERROR(get_logger(), "Receiver pipeline does not contain appsink named video_sink");
            stop_and_cleanup_pipeline();
            return false;
        }

        gst_app_sink_set_emit_signals(appsink_, FALSE);
        gst_app_sink_set_drop(appsink_, TRUE);
        gst_app_sink_set_max_buffers(appsink_, 1);

        bus_ = gst_element_get_bus(pipeline_);
        if (bus_ == nullptr)
        {
            RCLCPP_ERROR(get_logger(), "Failed to get GStreamer bus from receiver pipeline");
            stop_and_cleanup_pipeline();
            return false;
        }

        const GstStateChangeReturn result = gst_element_set_state(pipeline_, GST_STATE_PLAYING);
        if (result == GST_STATE_CHANGE_FAILURE)
        {
            RCLCPP_ERROR(get_logger(), "Failed to set receiver pipeline to PLAYING");
            stop_and_cleanup_pipeline();
            return false;
        }

        return true;
    }

    void drain_bus()
    {
        if (bus_ == nullptr)
        {
            return;
        }

        while (true)
        {
            GstMessage* message = gst_bus_pop_filtered(
                bus_,
                static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS | GST_MESSAGE_WARNING));
            if (message == nullptr)
            {
                break;
            }

            switch (GST_MESSAGE_TYPE(message))
            {
                case GST_MESSAGE_ERROR:
                {
                    GError* error = nullptr;
                    gchar* debug = nullptr;
                    gst_message_parse_error(message, &error, &debug);
                    RCLCPP_ERROR(
                        get_logger(),
                        "GStreamer receiver error: %s",
                        error != nullptr ? error->message : "unknown");
                    if (error != nullptr)
                    {
                        g_error_free(error);
                    }
                    g_free(debug);
                    running_.store(false);
                    break;
                }
                case GST_MESSAGE_EOS:
                    RCLCPP_WARN(get_logger(), "Receiver pipeline reached EOS");
                    running_.store(false);
                    break;
                case GST_MESSAGE_WARNING:
                {
                    GError* error = nullptr;
                    gchar* debug = nullptr;
                    gst_message_parse_warning(message, &error, &debug);
                    RCLCPP_WARN(
                        get_logger(),
                        "GStreamer receiver warning: %s",
                        error != nullptr ? error->message : "unknown");
                    if (error != nullptr)
                    {
                        g_error_free(error);
                    }
                    g_free(debug);
                    break;
                }
                default:
                    break;
            }

            gst_message_unref(message);
        }
    }

    void publish_sample(GstSample* sample)
    {
        GstCaps* caps = gst_sample_get_caps(sample);
        if (caps == nullptr)
        {
            RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000, "Received sample without caps");
            return;
        }

        GstStructure* structure = gst_caps_get_structure(caps, 0);
        int width = 0;
        int height = 0;
        if (!gst_structure_get_int(structure, "width", &width) ||
            !gst_structure_get_int(structure, "height", &height))
        {
            RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000, "Received sample without width/height");
            return;
        }

        GstBuffer* buffer = gst_sample_get_buffer(sample);
        if (buffer == nullptr)
        {
            RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000, "Received sample without buffer");
            return;
        }

        GstVideoInfo video_info;
        if (!gst_video_info_from_caps(&video_info, caps))
        {
            RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000, "Failed to parse video info from caps");
            return;
        }

        if (GST_VIDEO_INFO_N_PLANES(&video_info) < 1)
        {
            RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000, "Unexpected video planes in sample");
            return;
        }

        GstMapInfo map_info;
        if (!gst_buffer_map(buffer, &map_info, GST_MAP_READ))
        {
            RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000, "Failed to map sample buffer");
            return;
        }

        const int stride = GST_VIDEO_INFO_PLANE_STRIDE(&video_info, 0);
        if (stride <= 0)
        {
            gst_buffer_unmap(buffer, &map_info);
            RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 2000, "Invalid video stride in sample");
            return;
        }

        cv::Mat frame(height, width, CV_8UC3, map_info.data, static_cast<size_t>(stride));
        cv::Mat bgr = frame.clone();
        gst_buffer_unmap(buffer, &map_info);

        std_msgs::msg::Header header;
        header.stamp = now();
        header.frame_id = frame_id_;

        auto image_msg = cv_bridge::CvImage(header, "bgr8", bgr).toImageMsg();
        image_pub_->publish(*image_msg);

        sensor_msgs::msg::CameraInfo info_msg;
        info_msg.header = header;
        info_msg.width = static_cast<uint32_t>(width);
        info_msg.height = static_cast<uint32_t>(height);
        info_msg.distortion_model = "plumb_bob";
        info_msg.k = {1.0, 0.0, static_cast<double>(width) / 2.0,
                      0.0, 1.0, static_cast<double>(height) / 2.0,
                      0.0, 0.0, 1.0};
        info_msg.p = {1.0, 0.0, static_cast<double>(width) / 2.0, 0.0,
                      0.0, 1.0, static_cast<double>(height) / 2.0, 0.0,
                      0.0, 0.0, 1.0, 0.0};
        camera_info_pub_->publish(info_msg);
    }

    void bridge_loop()
    {
        while (rclcpp::ok() && running_.load())
        {
            GstSample* sample = gst_app_sink_try_pull_sample(appsink_, 200 * GST_MSECOND);
            if (sample == nullptr)
            {
                drain_bus();
                continue;
            }

            publish_sample(sample);
            gst_sample_unref(sample);
        }
    }

    std::atomic<bool> running_{false};
    std::thread worker_;

    std::string frame_id_;
    std::string receiver_pipeline_;

    GstElement* pipeline_ = nullptr;
    GstAppSink* appsink_ = nullptr;
    GstBus* bus_ = nullptr;

    rclcpp::Publisher<sensor_msgs::msg::Image>::SharedPtr image_pub_;
    rclcpp::Publisher<sensor_msgs::msg::CameraInfo>::SharedPtr camera_info_pub_;
};

int main(int argc, char* argv[])
{
    rclcpp::init(argc, argv);

    try
    {
        auto node = std::make_shared<PiCameraBridgeNode>();
        rclcpp::spin(node);
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Bridge node failed: " << ex.what() << std::endl;
        rclcpp::shutdown();
        return 1;
    }

    rclcpp::shutdown();
    return 0;
}
