from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    port_arg = DeclareLaunchArgument(
        'port',
        default_value='5000',
        description='UDP port to receive raw H.264 stream from Raspberry Pi',
    )

    rviz_config = PathJoinSubstitution([
        FindPackageShare('ros2_pi_camera_bridge'),
        'rviz',
        'camera.rviz',
    ])

    bridge_node = Node(
        package='ros2_pi_camera_bridge',
        executable='ros2_pi_camera_bridge_node',
        name='camera_bridge',
        output='screen',
        parameters=[{'port': LaunchConfiguration('port')}],
    )

    rviz_node = Node(
        package='rviz2',
        executable='rviz2',
        name='rviz2',
        arguments=['-d', rviz_config],
        output='screen',
    )

    return LaunchDescription([port_arg, bridge_node, rviz_node])
