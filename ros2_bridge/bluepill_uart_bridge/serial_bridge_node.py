from typing import Optional
import threading
import time

import rclpy
from rclpy.node import Node
from rclpy.utilities import ok as rclpy_ok
from std_msgs.msg import String

import serial
from serial import SerialException

from .protocol import (
    IncrementalParser,
    MessageType,
    ParseResult,
    ProtocolMessage,
    encode_frame,
)


class SerialBridgeNode(Node):
    def __init__(self) -> None:
        super().__init__("bluepill_serial_bridge")

        self.declare_parameter("port", "/dev/ttyUSB0")
        self.declare_parameter("baudrate", 115200)
        self.declare_parameter("poll_period_ms", 20)

        self.port = self.get_parameter("port").get_parameter_value().string_value
        self.baudrate = self.get_parameter("baudrate").get_parameter_value().integer_value
        poll_period_ms = self.get_parameter("poll_period_ms").get_parameter_value().integer_value
        self.poll_period_sec = poll_period_ms / 1000.0

        self.parser = IncrementalParser()
        self.serial_port: Optional[serial.Serial] = None
        self.last_open_attempt = 0.0
        self.reader_thread: Optional[threading.Thread] = None
        self.reader_stop = threading.Event()

        self.pub_raw = self.create_publisher(String, "bridge/rx_raw", 10)
        self.pub_data = self.create_publisher(String, "bridge/data", 10)
        self.pub_event = self.create_publisher(String, "bridge/event", 10)
        self.pub_status = self.create_publisher(String, "bridge/status", 10)
        self.pub_ack = self.create_publisher(String, "bridge/ack", 10)
        self.pub_error = self.create_publisher(String, "bridge/error", 10)
        self.pub_tx_raw = self.create_publisher(String, "bridge/tx_raw", 10)

        self.create_subscription(String, "bridge/tx_cmd", self.on_command, 10)
        self.create_subscription(String, "bridge/tx_frame", self.on_frame_request, 10)

        self.timer = self.create_timer(self.poll_period_sec, self.poll_serial)
        self.open_serial()

    def open_serial(self) -> None:
        now = time.monotonic()
        if (now - self.last_open_attempt) < 1.0:
            return

        self.last_open_attempt = now

        try:
            port = serial.serial_for_url(self.port, do_not_open=True)
            port.baudrate = self.baudrate
            port.timeout = None
            port.write_timeout = 0.5
            port.rtscts = False
            port.dsrdtr = False
            port.xonxoff = False
            port.dtr = False
            port.rts = False
            port.open()

            self.serial_port = port
            self.serial_port.reset_input_buffer()
            self.serial_port.reset_output_buffer()
            self.reader_stop.clear()
            self.reader_thread = threading.Thread(target=self.read_serial_loop, daemon=True)
            self.reader_thread.start()
            self.get_logger().info(f"Serial abierto en {self.port} @ {self.baudrate}")
        except (SerialException, OSError) as exc:
            self.serial_port = None
            self.get_logger().error(f"No se pudo abrir el puerto serie: {exc}")

    def close_serial(self, reason: str) -> None:
        self.reader_stop.set()

        if self.serial_port is not None:
            try:
                self.serial_port.close()
            except (SerialException, OSError):
                pass

        self.serial_port = None

        if (
            self.reader_thread is not None
            and self.reader_thread.is_alive()
            and self.reader_thread is not threading.current_thread()
        ):
            self.reader_thread.join(timeout=0.5)

        self.reader_thread = None
        if rclpy_ok():
            self.get_logger().warning(reason)

    def poll_serial(self) -> None:
        if self.serial_port is None:
            self.open_serial()

    def read_serial_loop(self) -> None:
        while not self.reader_stop.is_set():
            port = self.serial_port
            if port is None:
                return

            try:
                data = port.read(1)
            except TypeError:
                if self.reader_stop.is_set():
                    return
                self.close_serial("Error leyendo del puerto serie: descriptor invalido")
                return
            except (SerialException, OSError) as exc:
                self.close_serial(f"Error leyendo del puerto serie: {exc}")
                return

            if not data:
                continue

            try:
                result, message = self.parser.consume(chr(data[0]))
            except NotImplementedError as exc:
                self.get_logger().error(str(exc))
                self.close_serial("Parser UART todavía no implementado")
                return

            if result == ParseResult.MESSAGE_READY and message is not None:
                self.publish_message(message)
            elif result == ParseResult.ERROR:
                error_msg = String()
                error_msg.data = f"parser_error:0x{data[0]:02X}"
                self.pub_error.publish(error_msg)

    def publish_message(self, message: ProtocolMessage) -> None:
        raw_msg = String()
        raw_msg.data = f"{message.msg_type.value}:{message.payload}"
        self.pub_raw.publish(raw_msg)

        typed_msg = String()
        typed_msg.data = message.payload

        if message.msg_type == MessageType.DAT:
            self.pub_data.publish(typed_msg)
        elif message.msg_type == MessageType.EVT:
            self.pub_event.publish(typed_msg)
        elif message.msg_type == MessageType.STS:
            self.pub_status.publish(typed_msg)
        elif message.msg_type == MessageType.ACK:
            self.pub_ack.publish(typed_msg)
        elif message.msg_type == MessageType.ERR:
            self.pub_error.publish(typed_msg)

    def send_protocol_message(self, message: ProtocolMessage) -> None:
        if self.serial_port is None:
            self.get_logger().warning("Puerto serie no disponible")
            return

        try:
            frame = encode_frame(message)
        except NotImplementedError as exc:
            self.get_logger().error(str(exc))
            return
        except ValueError as exc:
            self.get_logger().warning(f"Mensaje inválido: {exc}")
            return

        try:
            self.serial_port.write(frame.encode("ascii"))
            self.serial_port.flush()
            tx_msg = String()
            tx_msg.data = frame.rstrip("\n")
            self.pub_tx_raw.publish(tx_msg)
        except (SerialException, OSError) as exc:
            self.close_serial(f"No se pudo transmitir la trama: {exc}")

    def on_command(self, msg: String) -> None:
        self.send_protocol_message(ProtocolMessage(MessageType.CMD, msg.data))

    def on_frame_request(self, msg: String) -> None:
        try:
            msg_type_text, payload = msg.data.split(":", 1)
            msg_type = MessageType(msg_type_text)
        except ValueError:
            self.get_logger().warning("Formato esperado: TTT:payload")
            return
        except Exception as exc:
            self.get_logger().warning(f"No se pudo interpretar bridge/tx_frame: {exc}")
            return

        self.send_protocol_message(ProtocolMessage(msg_type, payload))


def main(args=None) -> None:
    rclpy.init(args=args)
    node = SerialBridgeNode()
    try:
        rclpy.spin(node)
    finally:
        node.close_serial("Cerrando bridge serie")
        node.destroy_node()
        if rclpy_ok():
            rclpy.shutdown()
