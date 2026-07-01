# Diagnostico UART y solucion del HardFault

Esta guia documenta el problema encontrado durante la integracion Blue Pill <-> USB-UART <-> ROS 2, como se diagnostico y cual fue la solucion aplicada.

## Sintoma inicial

La PC con ROS 2 podia abrir `/dev/ttyUSB0` y el bridge publicaba tramas TX validas, por ejemplo:

```text
@08:CMD:ping:52
@0A:CMD:led=on:6A
```

La Blue Pill tambien transmitia telemetria hacia la PC:

```text
@13:DAT:temp=25,seq=135:24
@2B:STS:rx=0,ae=0,irq=108,pb=108,pm=0,pe=0,qd=0:42
```

Pero los comandos no eran procesados de forma estable. En algunos intentos el status indicaba:

```text
rx=0,ae=0,irq=0,pb=0,pm=0,pe=0,qd=0
```

Eso parecia indicar que la Blue Pill no recibia bytes por `PA10`.

## Hipotesis revisadas

Se verificaron estas capas:

- Puerto serie correcto: `/dev/ttyUSB0`.
- Baudrate correcto: `115200`.
- Protocolo correcto: Python generaba tramas validas.
- Cableado esperado: `PA9 -> RX`, `PA10 -> TX`, `GND -> GND`.
- ST-Link funcional.
- Firmware corriendo FreeRTOS.
- Bridge ROS 2 abriendo el puerto.

La prueba directa con ST-Link mostro que algunos bytes si entraban por USART1, pero luego el micro quedaba detenido.

## Prueba decisiva

Se uso:

```bash
scripts/debug_uart_rx_check.sh /dev/ttyUSB0
```

El script:

1. abre OpenOCD,
2. resetea la Blue Pill,
3. envia tramas validas por `/dev/ttyUSB0`,
4. interrumpe la CPU,
5. lee contadores internos por GDB.

Antes de la correccion, GDB mostro que el firmware caia en `HardFault`:

```text
blocking_handler () at vector.c:104
current mode: Handler HardFault
```

El backtrace relevante fue:

```text
#0 blocking_handler
#2 xTaskRemoveFromEventList
#3 xQueueGenericSendFromISR
#4 usart1_isr
#10 parse_hex_byte
#11 parser_consume_byte
#12 task_parser
```

La informacion importante es que el fallo no estaba en ROS 2. El micro estaba entrando a HardFault al combinar:

- recepcion UART por interrupcion,
- `xQueueSendFromISR`,
- tarea parser,
- conversion hexadecimal con `sscanf`.

## Causa raiz

El parser C usaba:

```c
sscanf(text, "%2X", &value);
```

para convertir campos hexadecimales de dos caracteres.

En un microcontrolador chico, `sscanf` es una dependencia pesada:

- arrastra partes de newlib,
- usa mas stack que una conversion manual,
- complica la reentrancia,
- no es apropiado para una ruta critica de parser byte-a-byte con tareas chicas.

La tarea `task_parser` tenia stack limitado. En la practica, al recibir bytes por UART y activar el scheduler desde la ISR, el sistema podia terminar con corrupcion de pila o HardFault.

## Solucion aplicada

Se reemplazo `sscanf` por conversion hexadecimal manual en `firmware/protocol/parser.c`.

Antes:

```c
static uint8_t parse_hex_byte(const char *text)
{
    unsigned int value = 0U;

    (void) sscanf(text, "%2X", &value);
    return (uint8_t) value;
}
```

Despues:

```c
static uint8_t hex_digit_value(char digit)
{
    if ((digit >= '0') && (digit <= '9')) {
        return (uint8_t) (digit - '0');
    }

    return (uint8_t) (10U + (uint8_t) (digit - 'A'));
}

static uint8_t parse_hex_byte(const char *text)
{
    return (uint8_t) ((hex_digit_value(text[0]) << 4) | hex_digit_value(text[1]));
}
```

El parser ya valida previamente que ambos caracteres sean hexadecimales (`0-9`, `A-F`), por eso la funcion manual no necesita manejar otros casos.

## Validacion posterior

Despues de recompilar y flashear:

```bash
cd firmware
make
make flash
```

La prueba directa:

```bash
python3 scripts/uart_link_check.py bluepill /dev/ttyUSB0
```

mostro respuestas correctas:

```text
@0A:ACK:pong=1:22
@29:STS:rx=2,ae=0,irq=35,pb=35,pm=2,pe=0,qd=0:39
@0A:ACK:cmd=ok:6B
```

Luego se valido ROS 2:

```bash
ros2 topic pub --once /bridge/tx_cmd std_msgs/msg/String "{data: ping}"
ros2 topic echo /bridge/ack
```

Resultado:

```text
data: pong=1
---
```

Tambien se recibio telemetria:

```text
data: temp=29,seq=749
---
```

y estado:

```text
data: rx=7,ae=0,irq=112,pb=112,pm=7,pe=0,qd=0
---
```

## Como diagnosticar si vuelve a fallar

### 1. Ver si el adaptador existe

```bash
ls -l /dev/ttyUSB* /dev/ttyACM*
```

### 2. Ver permisos

```bash
id
ls -l /dev/ttyUSB0
```

El usuario debe pertenecer a `dialout`.

### 3. Probar loopback del adaptador

Desconectar la Blue Pill y unir TX con RX del adaptador:

```bash
python3 scripts/uart_link_check.py loopback /dev/ttyUSB0
```

Si falla, el problema esta en adaptador, driver, puerto o permisos.

### 4. Probar Blue Pill sin ROS 2

```bash
python3 scripts/uart_link_check.py bluepill /dev/ttyUSB0
```

Si hay `DAT` pero no hay `ACK`, revisar `PA10`, TX del adaptador y GND comun.

Si hay `ACK`, el camino serie funciona y el problema esta en ROS 2 o en como se esta ejecutando el bridge.

### 5. Leer contadores por ST-Link

```bash
scripts/debug_uart_rx_check.sh /dev/ttyUSB0
```

Interpretacion:

- `g_uart_rx_irq_count` sube: llegan bytes al periferico USART1.
- `g_parser_byte_count` sube: la tarea parser consume bytes.
- `g_parser_message_count` sube: hay tramas validas.
- `g_rx_count` sube: la app proceso comandos.
- `g_parser_error_count` sube: hay tramas mal formadas, baudrate incorrecto o ruido.
- PC en `blocking_handler`/HardFault: bug de firmware o stack insuficiente.

## Recomendaciones para alumnos

- Evitar `scanf`, `sscanf`, `printf` extensivo y conversiones pesadas en tareas con poco stack.
- Preferir parsers manuales para protocolos simples.
- Medir por capas: loopback, UART directa, contadores por ST-Link, ROS 2.
- No culpar a ROS 2 antes de verificar que el micro recibe y procesa bytes.
- Documentar cada comando nuevo con ejemplo de trama y topico ROS 2 asociado.
# Troubleshooting UART

## Síntoma: no aparece `/dev/ttyUSB0`

Posibles causas:

- adaptador USB-UART no conectado,
- driver del sistema no cargado,
- WSL2 sin passthrough USB,
- el adaptador aparece con otro nombre (`/dev/ttyUSB1`, `/dev/ttyACM0`).

Probar:

```bash
ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
dmesg | tail -n 30
```

## Síntoma: `Permission denied` al abrir el puerto

Agregar el usuario a `dialout`:

```bash
sudo usermod -aG dialout "$USER"
```

Cerrar sesión y volver a entrar.

## Síntoma: `make flash` falla

Revisar:

- ST-Link conectado,
- alimentación correcta,
- cableado SWD,
- OpenOCD instalado.

Probar:

```bash
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg
```

## Síntoma: el bridge levanta pero no hay respuestas

Revisar:

- cableado TX/RX cruzado,
- GND común,
- baudrate correcto,
- implementación de `encode_frame(...)`,
- implementación del parser incremental,
- checksum XOR,
- longitud `LL` correcta.

## Síntoma: llegan bytes pero no mensajes válidos

Eso suele indicar un error de protocolo.

Checklist:

1. ¿La trama empieza con `@`?
2. ¿`LL` coincide con la longitud real de `TTT:PAYLOAD`?
3. ¿Hay `:` en los lugares correctos?
4. ¿`CC` es el XOR de `LL:TTT:PAYLOAD`?
5. ¿La trama termina en `\n`?
6. ¿El parser vuelve a estado inicial ante error?

## Síntoma: desde ROS 2 se publica pero la placa no hace nada

Revisar:

- que `task_uart_tx` esté generando una trama válida,
- que `task_parser` de firmware entregue `PARSER_RESULT_MESSAGE_READY`,
- que `app_handle_message(...)` reconozca el `CMD` recibido,
- que el bridge realmente esté escribiendo por serie.

## Herramientas útiles

Loopback del adaptador:

```bash
python3 scripts/uart_link_check.py loopback /dev/ttyUSB0
```

Prueba básica con Blue Pill:

```bash
python3 scripts/uart_link_check.py bluepill /dev/ttyUSB0
```

Debug con GDB/OpenOCD:

```bash
scripts/debug_uart_rx_check.sh /dev/ttyUSB0
```
