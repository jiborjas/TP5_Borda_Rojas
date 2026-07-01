# Práctica: implementar UART + ROS 2

## Objetivo

Completar el protocolo que une:

- la Blue Pill con FreeRTOS,
- el bridge en ROS 2,
- y los tópicos que usa la PC.

## Archivos a completar

### Firmware

- `firmware/protocol/protocol.c`
- `firmware/protocol/parser.c`

### ROS 2

- `ros2_bridge/bluepill_uart_bridge/protocol.py`
- `ros2_bridge/bluepill_uart_bridge/serial_bridge_node.py`

## Metas mínimas

### Meta 1

Poder construir una trama válida a partir de un mensaje lógico.

Ejemplo esperado:

```text
CMD + ping -> @08:CMD:ping:52\n
```

### Meta 2

Poder recibir una trama byte a byte y reconstruir el mensaje original.

### Meta 3

Poder publicar desde ROS 2:

```bash
ros2 topic pub --once /bridge/tx_cmd std_msgs/msg/String "{data: ping}"
```

y obtener una respuesta válida desde la Blue Pill.

### Meta 4

Ver telemetría en:

```bash
ros2 topic echo /bridge/data
```

## Estrategia sugerida

1. Resolver primero `compute_checksum` / `protocol_compute_checksum`.
2. Resolver después `encode_frame` / `protocol_encode_frame`.
3. Resolver `decode_body`.
4. Recién ahí implementar el parser incremental.
5. Finalmente unir todo con UART real.

## Casos que deberían contemplar

- payload vacío,
- payload al máximo permitido,
- checksum incorrecto,
- longitud inválida,
- bytes basura antes de `@`,
- recepción fragmentada,
- reinicio del parser ante error.

## Pistas importantes

- La longitud `LL` cuenta solo `TTT:PAYLOAD`.
- El checksum se calcula sobre `LL:TTT:PAYLOAD`.
- El parser no debe asumir que la trama llega toda junta.
- UART es un flujo de bytes, no una lista de mensajes.

## Comandos ROS 2 que van a usar mucho

Levantar el bridge:

```bash
ros2 run bluepill_uart_bridge serial_bridge_node --ros-args -p port:=/dev/ttyUSB0 -p baudrate:=115200
```

Ver tópicos:

```bash
ros2 topic list
```

Ver ACKs:

```bash
ros2 topic echo /bridge/ack
```

Ver errores:

```bash
ros2 topic echo /bridge/error
```

Enviar comando:

```bash
ros2 topic pub --once /bridge/tx_cmd std_msgs/msg/String "{data: ping}"
```

Enviar trama lógica completa:

```bash
ros2 topic pub --once /bridge/tx_frame std_msgs/msg/String "{data: CMD:status?}"
```

## Señal de que van bien

Si todo está bien implementado, deberían ver:

- tramas válidas saliendo por UART,
- `ACK` para `ping`,
- `STS` para `status?`,
- telemetría periódica por `/bridge/data`.
