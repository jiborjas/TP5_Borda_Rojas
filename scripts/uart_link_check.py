#!/usr/bin/env python3
import argparse
import sys
import time

import serial


FRAMES = (
    b"@08:CMD:ping:52\n",
    b"@0B:CMD:status?:13\n",
    b"@0E:CMD:led=toggle:7D\n",
)


def open_port(port: str, baudrate: int) -> serial.Serial:
    return serial.Serial(
        port=port,
        baudrate=baudrate,
        timeout=0.25,
        write_timeout=0.5,
        rtscts=False,
        dsrdtr=False,
        xonxoff=False,
    )


def read_for(ser: serial.Serial, seconds: float) -> bytes:
    deadline = time.monotonic() + seconds
    data = bytearray()
    while time.monotonic() < deadline:
        chunk = ser.read(128)
        if chunk:
            data.extend(chunk)
    return bytes(data)


def run_loopback(args: argparse.Namespace) -> int:
    payload = b"bluepill-uart-loopback\n"
    with open_port(args.port, args.baudrate) as ser:
        ser.reset_input_buffer()
        ser.reset_output_buffer()
        ser.write(payload)
        ser.flush()
        received = read_for(ser, args.timeout)

    if payload in received:
        print("OK: loopback USB-UART funciona.")
        return 0

    print("ERROR: no volvio el payload de loopback.")
    print("Conecta TX y RX del adaptador entre si, sin la Blue Pill, y repeti la prueba.")
    if received:
        print(f"Bytes recibidos: {received!r}")
    return 1


def run_bluepill(args: argparse.Namespace) -> int:
    with open_port(args.port, args.baudrate) as ser:
        ser.reset_input_buffer()
        initial = read_for(ser, args.listen)
        for frame in FRAMES:
            print(f"TX: {frame.decode('ascii').rstrip()}")
            ser.write(frame)
            ser.flush()
            time.sleep(0.2)
        received = initial + read_for(ser, args.timeout)

    if received:
        print("RX:")
        print(received.decode("ascii", "replace").rstrip())
    else:
        print("RX: sin datos")

    if b"ACK" in received or b"STS" in received:
        print("OK: hay respuesta desde la Blue Pill hacia la PC.")
        print("Si el status sigue mostrando irq=0, la placa transmite pero no recibe por PA10.")
        return 0

    print("ERROR: no se recibieron ACK/STS desde la Blue Pill.")
    print("Revisar firmware flasheado, PA9->RX del adaptador, GND comun y puerto correcto.")
    return 1


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Diagnostico UART USB-UART/Blue Pill.")
    parser.add_argument("mode", choices=("loopback", "bluepill"))
    parser.add_argument("port", nargs="?", default="/dev/ttyUSB0")
    parser.add_argument("--baudrate", type=int, default=115200)
    parser.add_argument("--timeout", type=float, default=2.0)
    parser.add_argument("--listen", type=float, default=1.5)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        if args.mode == "loopback":
            return run_loopback(args)
        return run_bluepill(args)
    except serial.SerialException as exc:
        print(f"ERROR serie: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
