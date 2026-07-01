from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument("port", default_value="/dev/ttyUSB0"),
        DeclareLaunchArgument("baudrate", default_value="115200"),
        DeclareLaunchArgument("poll_period_ms", default_value="20"),
        Node(
            package="bluepill_uart_bridge",
            executable="serial_bridge_node",
            name="bluepill_serial_bridge",
            parameters=[
                {"port": LaunchConfiguration("port")},
                {"baudrate": LaunchConfiguration("baudrate")},
                {"poll_period_ms": LaunchConfiguration("poll_period_ms")},
            ],
        )
    ])
