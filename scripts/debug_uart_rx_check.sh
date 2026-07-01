#!/usr/bin/env bash

set -euo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
FIRMWARE_DIR="$REPO_ROOT/firmware"
PORT="${1:-/dev/ttyUSB0}"
OPENOCD_LOG="/tmp/debug_uart_rx_check_openocd.log"
GDB_LOG="/tmp/debug_uart_rx_check_gdb.log"

cleanup() {
  if [[ -n "${OPENOCD_PID:-}" ]] && kill -0 "$OPENOCD_PID" 2>/dev/null; then
    kill "$OPENOCD_PID" 2>/dev/null || true
    wait "$OPENOCD_PID" 2>/dev/null || true
  fi
}

trap cleanup EXIT

if ! command -v openocd >/dev/null 2>&1; then
  printf 'Falta openocd\n' >&2
  exit 1
fi

if ! command -v gdb-multiarch >/dev/null 2>&1; then
  printf 'Falta gdb-multiarch\n' >&2
  exit 1
fi

if ! command -v python3 >/dev/null 2>&1; then
  printf 'Falta python3\n' >&2
  exit 1
fi

if [[ ! -e "$PORT" ]]; then
  printf 'No existe el puerto %s\n' "$PORT" >&2
  exit 1
fi

if [[ ! -f "$FIRMWARE_DIR/bin/main.elf" ]]; then
  printf 'No existe %s/bin/main.elf. Compila el firmware primero.\n' "$FIRMWARE_DIR" >&2
  exit 1
fi

printf 'Usando puerto: %s\n' "$PORT"

openocd -f interface/stlink.cfg -f target/stm32f1x.cfg >"$OPENOCD_LOG" 2>&1 &
OPENOCD_PID=$!
sleep 2

gdb-multiarch -batch "$FIRMWARE_DIR/bin/main.elf" \
  -ex 'target extended-remote :3333' \
  -ex 'monitor reset run' \
  -ex "shell python3 - <<'PY'
import serial
import time

port = '$PORT'
frames = (
    b'@0A:CMD:led=on:6A\n',
    b'@08:CMD:ping:52\n',
    b'@0B:CMD:status?:13\n',
)

ser = serial.Serial(port, 115200, timeout=0.2, write_timeout=0.5)
ser.reset_input_buffer()
for frame in frames:
    ser.write(frame)
    ser.flush()
    time.sleep(0.2)
ser.close()
PY" \
  -ex 'shell sleep 1' \
  -ex 'interrupt' \
  -ex 'printf "\nContadores UART/parser en RAM:\n"' \
  -ex 'printf "g_parser_byte_count   = "' \
  -ex 'p/u g_parser_byte_count' \
  -ex 'printf "g_parser_message_count= "' \
  -ex 'p/u g_parser_message_count' \
  -ex 'printf "g_parser_error_count  = "' \
  -ex 'p/u g_parser_error_count' \
  -ex 'printf "g_uart_rx_irq_count   = "' \
  -ex 'p/u g_uart_rx_irq_count' \
  -ex 'printf "g_uart_rx_drop_count  = "' \
  -ex 'p/u g_uart_rx_drop_count' \
  -ex 'printf "g_rx_count            = "' \
  -ex 'p/u g_rx_count' \
  -ex 'printf "g_error_count         = "' \
  -ex 'p/u g_error_count' \
  -ex 'printf "\nPC actual:\n"' \
  -ex 'info registers pc xpsr' \
  -ex 'monitor reset run' \
  -ex 'quit' >"$GDB_LOG" 2>&1

printf '\nOpenOCD log: %s\n' "$OPENOCD_LOG"
printf 'GDB log: %s\n\n' "$GDB_LOG"
cat "$GDB_LOG"
