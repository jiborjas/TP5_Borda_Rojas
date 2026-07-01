from setuptools import setup

package_name = "bluepill_uart_bridge"

setup(
    name=package_name,
    version="0.1.0",
    packages=[package_name],
    data_files=[
        ("share/ament_index/resource_index/packages", [f"resource/{package_name}"]),
        (f"share/{package_name}", ["package.xml"]),
        (f"share/{package_name}/launch", ["launch/bridge.launch.py"]),
    ],
    install_requires=["setuptools", "pyserial"],
    zip_safe=True,
    maintainer="Docente",
    maintainer_email="docente@example.com",
    description="Student ROS 2 bridge scaffold for Blue Pill UART protocol practice",
    license="MIT",
    entry_points={
        "console_scripts": [
            "serial_bridge_node = bluepill_uart_bridge.serial_bridge_node:main",
        ],
    },
)
