# Enunciado del trabajo práctico

## Objetivo general

Desarrollar un sistema embebido sobre una **Blue Pill STM32F103C8T6** usando **FreeRTOS**, con integración hacia una PC mediante **UART + ROS 2**.

La cátedra les entrega una base con:

- estructura de firmware,
- tareas FreeRTOS,
- drivers UART,
- aplicación inicial,
- paquete ROS 2,
- documentación completa.

Lo que ustedes deben resolver es la **implementación del protocolo UART** que conecta ambos mundos.

## Qué se espera que implementen

### Lado firmware

- armado de tramas UART,
- cálculo/validación de checksum,
- parseo incremental byte a byte,
- reconstrucción de mensajes válidos,
- manejo básico de errores de protocolo.

### Lado ROS 2

- codificación de mensajes hacia UART,
- parser incremental de bytes recibidos,
- traducción entre UART y tópicos ROS 2,
- publicación por tópico según tipo de mensaje.

## Requisitos mínimos

1. Mantener la arquitectura modular provista.
2. Usar FreeRTOS con múltiples tareas.
3. Implementar el protocolo UART documentado en `docs/PROTOCOL.md`.
4. Lograr comunicación bidireccional Blue Pill ↔ ROS 2.
5. Integrar al menos **2 dispositivos físicos** en la aplicación final.
6. Documentar qué comandos soporta el sistema.
7. Presentar una demo funcional.

## Archivos que deben completar

- `firmware/protocol/protocol.c`
- `firmware/protocol/parser.c`
- `ros2_bridge/bluepill_uart_bridge/protocol.py`
- `ros2_bridge/bluepill_uart_bridge/serial_bridge_node.py`

## Recomendación de orden de trabajo

1. Hacer funcionar el loopback del adaptador USB-UART.
2. Implementar encode/decode en un solo lado.
3. Implementar parser incremental.
4. Validar `ping`.
5. Validar `status?`.
6. Validar telemetría periódica.
7. Recién después extender sensores/actuadores.

## Entregables sugeridos

1. Código fuente completo.
2. README del proyecto del grupo.
3. Lista de comandos soportados.
4. Diagrama simple de arquitectura.
5. Demo o video de funcionamiento.

## Criterios de evaluación

- robustez del protocolo,
- claridad de código,
- correcta separación por módulos,
- buen uso de FreeRTOS,
- calidad de la integración hardware/software,
- documentación,
- estabilidad de la demo.

## Consejos

- No meter toda la lógica en `main.c`.
- No parsear dentro de la ISR.
- No mezclar hardware con lógica de aplicación.
- Probar primero UART directa y después ROS 2.
- Si falla, revisar longitud, checksum y resincronización.
