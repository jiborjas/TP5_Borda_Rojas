# Arquitectura general

## Idea principal

El repositorio ya resuelve la base del sistema, pero deja sin implementar la parte más importante de la práctica: **el protocolo UART entre firmware y ROS 2**.

La idea es que ustedes puedan concentrarse en esa integración sin tener que empezar de cero con clocks, RTOS, colas, ISR, package ROS 2 o cableado.

## Bloques del sistema

### 1. Firmware en la Blue Pill

- `main.c`: arranque del sistema.
- `platform/`: setup básico de placa.
- `drivers/`: acceso a UART.
- `app/`: lógica de aplicación, sensores y actuadores.
- `protocol/`: framing, checksum y parser incremental **(a completar por ustedes)**.

### 2. Bridge ROS 2 en la PC

- abre el puerto serie,
- recibe comandos por tópico,
- arma tramas UART,
- parsea respuestas de la Blue Pill,
- publica por tópicos ROS 2.

La estructura del paquete ROS 2 está lista, pero la lógica de protocolo quedó como scaffold.

## Estructura del firmware

```text
firmware/
├── app/
│   ├── actuators.c
│   ├── actuators.h
│   ├── app.c
│   ├── app.h
│   ├── sensors.c
│   ├── sensors.h
│   ├── tasks.c
│   └── tasks.h
├── config/
│   ├── app_config.h
│   └── FreeRTOSConfig.h
├── drivers/
│   ├── uart_comm.c
│   └── uart_comm.h
├── platform/
│   ├── system_init.c
│   └── system_init.h
├── protocol/
│   ├── parser.c        <- completar
│   ├── parser.h
│   ├── protocol.c      <- completar
│   └── protocol.h
├── third_party/
├── main.c
└── Makefile
```

## Responsabilidad de cada módulo

- `main.c`: inicializa clocks, UART, sensores y scheduler.
- `drivers/uart_comm.c`: configura USART1 y captura bytes en interrupción.
- `app/tasks.c`: conecta ISR, parser, aplicación, actuadores y TX.
- `app/app.c`: interpreta mensajes de alto nivel (`CMD`, `STS`, `DAT`, etc.).
- `protocol/*.c`: transforma bytes en mensajes y mensajes en tramas.

## Flujo de datos en firmware

```text
USART1 ISR -> cola RX ISR -> task_uart_rx -> task_parser -> task_app
                                                       -> task_uart_tx -> UART
sensors -> task_telemetry -------------------------------------------> UART
```

## Qué ya está resuelto

- cola desde ISR hacia tarea,
- tareas separadas,
- generación de telemetría desde la app,
- comandos de aplicación de ejemplo,
- publicación/suscripción ROS 2 esperada.

## Qué falta a propósito

### En C

- `protocol_encode_frame(...)`
- `protocol_decode_body(...)`
- `protocol_compute_checksum(...)`
- `parser_consume_byte(...)`

### En Python

- `compute_checksum(...)`
- `encode_frame(...)`
- `decode_body(...)`
- `IncrementalParser.consume(...)`
- integración completa del nodo con el protocolo.

## Flujo de datos completo

```text
ROS 2 topic -> bridge ROS 2 -> frame UART -> USART1 RX -> parser -> app
app -> mensaje lógico -> frame UART -> bridge ROS 2 -> tópico ROS 2
```

## Estrategia recomendada

1. Implementar encode/decode sin UART real.
2. Implementar parser incremental.
3. Verificar tramas conocidas a mano.
4. Integrar parser al firmware.
5. Integrar parser al bridge ROS 2.
6. Probar extremo a extremo.
