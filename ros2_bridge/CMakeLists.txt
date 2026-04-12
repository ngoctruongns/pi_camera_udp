cmake_minimum_required(VERSION 3.8)
project(ros2_pi_camera_bridge)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ament_cmake REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(OpenCV REQUIRED)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0 gstreamer-app-1.0)
pkg_check_modules(GSTREAMER_VIDEO REQUIRED gstreamer-video-1.0)

link_directories(${GSTREAMER_LIBRARY_DIRS})

add_executable(ros2_pi_camera_bridge_node src/bridge_node.cpp)

target_include_directories(ros2_pi_camera_bridge_node PRIVATE
  include
  ${OpenCV_INCLUDE_DIRS}
  ${GSTREAMER_INCLUDE_DIRS}
  ${GSTREAMER_VIDEO_INCLUDE_DIRS}
)

target_compile_options(ros2_pi_camera_bridge_node PRIVATE ${GSTREAMER_CFLAGS_OTHER})
target_compile_options(ros2_pi_camera_bridge_node PRIVATE ${GSTREAMER_VIDEO_CFLAGS_OTHER})

ament_target_dependencies(ros2_pi_camera_bridge_node
  rclcpp
  sensor_msgs
  std_msgs
  cv_bridge
)

target_link_libraries(ros2_pi_camera_bridge_node
  ${OpenCV_LIBS}
  ${GSTREAMER_LIBRARIES}
  ${GSTREAMER_VIDEO_LIBRARIES}
)

install(TARGETS ros2_pi_camera_bridge_node
  DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY launch rviz
  DESTINATION share/${PROJECT_NAME}
)

ament_package()
