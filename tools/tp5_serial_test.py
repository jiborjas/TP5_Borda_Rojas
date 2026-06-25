#!/usr/bin/env python3
"""Herramienta de prueba serie para TP5.

Uso sin placa:
    python3 tools/tp5_serial_test.py --encode-only

Uso con placa:
    python3 tools/tp5_serial_test.py --port /dev/ttyUSB0
"""

from __future__ import annotations

import argparse
import sys
import time


START_CHAR = "@"
END_CHAR = "\n"
SEPARATOR = ":"


def checksum(text: str) -> int:
    """Calcula XOR byte a byte sobre LL:TTT:PAYLOAD."""

    # El acumulador arranca en cero.
    value = 0

    # El protocolo trabaja con ASCII.
    for byte in text.encode("ascii"):
        # XOR acumulativo.
        value ^= byte

    # Devuelve 0..255.
    return value


def encode_frame(msg_type: str, payload: str) -> str:
    """Arma @LL:TTT:PAYLOAD:CC\\n."""

    # El body es lo que cuenta LL.
    body = f"{msg_type}{SEPARATOR}{payload}"

    # LL es longitud de body en hexadecimal de dos digitos.
    length_field = f"{len(body):02X}"

    # El checksum protege LL:body.
    protected = f"{length_field}{SEPARATOR}{body}"

    # CC es XOR sobre protected.
    check = checksum(protected)

    # Trama completa para UART.
    return f"{START_CHAR}{protected}{SEPARATOR}{check:02X}{END_CHAR}"


def default_commands() -> list[tuple[str, str, str]]:
    """Comandos exigidos por el PDF y respuesta esperada."""

    # Cada tupla es tipo, payload y tipo de respuesta esperada.
    return [
        ("CMD", "ping", "ACK"),
        ("CMD", "led=on", "ACK"),
        ("CMD", "led=off", "ACK"),
        ("CMD", "led=toggle", "ACK"),
        ("CMD", "status?", "STS"),
        ("CMD", "comando_raro", "ERR"),
        ("DAT", "temp=25", "ERR"),
    ]


def print_reference_frames() -> None:
    """Imprime tramas listas para copiar en picocom/minicom."""

    # Recorremos los comandos principales.
    for msg_type, payload, expected in default_commands():
        # Codificamos igual que el firmware.
        frame = encode_frame(msg_type, payload)

        # rstrip solo evita que print agregue una linea en blanco extra.
        print(f"{msg_type}:{payload:14s} -> {frame.rstrip()}  espera {expected}")

    # Caso de resincronizacion pedido por el PDF.
    noisy = "@0" + encode_frame("CMD", "ping")

    # Esta trama deberia producir un error de parser y luego ACK:pong=1.
    print(f"resync             -> {noisy.rstrip()}  espera error + ACK")


def read_until_expected(ser, expected_type: str, timeout_s: float) -> list[str]:
    """Lee lineas hasta encontrar una del tipo esperado o agotar timeout."""

    # Marca de tiempo final.
    deadline = time.monotonic() + timeout_s

    # Lineas crudas recibidas.
    lines: list[str] = []

    # Mientras quede tiempo seguimos leyendo.
    while time.monotonic() < deadline:
        # readline espera hasta '\n' o timeout interno del puerto.
        raw = ser.readline()

        # Si no llego nada, seguimos hasta deadline.
        if not raw:
            continue

        # Decodificamos sin romper el script por bytes raros.
        line = raw.decode("ascii", errors="replace").strip()

        # Guardamos para mostrar evidencia.
        lines.append(line)

        # Una respuesta esperada se ve como @LL:TTT:PAYLOAD:CC.
        if f":{expected_type}:" in line:
            break

    # Devolvemos todo lo visto para que el usuario lo guarde como evidencia.
    return lines


def run_serial(port: str, baudrate: int, timeout: float) -> int:
    """Abre el puerto serie, manda comandos y muestra respuestas."""

    # pyserial no es dependencia obligatoria para --encode-only.
    try:
        import serial
    except ImportError:
        print("Falta pyserial. Instalalo con: python3 -m pip install pyserial", file=sys.stderr)
        return 2

    # Abrimos el puerto con 115200 8N1 por defecto.
    with serial.Serial(port, baudrate=baudrate, timeout=0.2) as ser:
        # Pequeña pausa para que el adaptador quede estable.
        time.sleep(0.2)

        # Tiramos bytes viejos antes de empezar la prueba.
        ser.reset_input_buffer()

        # Probamos todos los comandos principales.
        for msg_type, payload, expected in default_commands():
            # Codificamos la trama.
            frame = encode_frame(msg_type, payload)

            # Mostramos TX como evidencia.
            print(f"TX {frame.rstrip()}")

            # Enviamos ASCII por UART.
            ser.write(frame.encode("ascii"))

            # Esperamos una respuesta del tipo esperado.
            lines = read_until_expected(ser, expected, timeout)

            # Mostramos todo lo recibido porque puede haber DAT/STS periodicos intercalados.
            for line in lines:
                print(f"RX {line}")

        # Caso extra: resincronizacion con '@' en medio de una trama rota.
        noisy = "@0" + encode_frame("CMD", "ping")

        # Mostramos TX.
        print(f"TX {noisy.rstrip()}")

        # Enviamos ruido + trama valida.
        ser.write(noisy.encode("ascii"))

        # Esperamos ACK luego de la resincronizacion.
        lines = read_until_expected(ser, "ACK", timeout)

        # Mostramos evidencia.
        for line in lines:
            print(f"RX {line}")

    # Codigo cero: script completo sin excepciones.
    return 0


def main() -> int:
    """Parsea argumentos y ejecuta el modo pedido."""

    # Parser de linea de comandos.
    parser = argparse.ArgumentParser(description="Prueba UART para TP5")

    # Puerto serie opcional.
    parser.add_argument("--port", help="Puerto serie, por ejemplo /dev/ttyUSB0 o COM5")

    # Baudrate configurable por si el laboratorio usa otro.
    parser.add_argument("--baudrate", type=int, default=115200, help="Baudrate UART")

    # Timeout total por comando.
    parser.add_argument("--timeout", type=float, default=2.0, help="Timeout por comando en segundos")

    # Modo que no abre el puerto.
    parser.add_argument("--encode-only", action="store_true", help="Solo imprime tramas de referencia")

    # Leemos argumentos.
    args = parser.parse_args()

    # Sin puerto o con --encode-only, imprimimos tramas y salimos.
    if args.encode_only or args.port is None:
        print_reference_frames()
        return 0

    # Con puerto, probamos contra la placa.
    return run_serial(args.port, args.baudrate, args.timeout)


if __name__ == "__main__":
    raise SystemExit(main())
