# Generación y parsing de mensajes UART

Este documento explica en detalle cómo se generan y cómo se parsean los mensajes del protocolo UART usado por el proyecto.

El parser usa una **FSM** (*Finite State Machine*, máquina de estados finitos). La FSM procesa la trama byte por byte y avanza entre estados según lo que espera recibir.

## Objetivo del protocolo

La comunicación serie por UART no preserva mensajes. UART entrega un flujo continuo de bytes:

```text
@08:CMD:ping:52\n@0A:ACK:pong=1:22\n...
```

El receptor no sabe automáticamente dónde empieza o termina cada mensaje. Por eso el proyecto define un formato de trama:

```text
@LL:TTT:PAYLOAD:CC\n
```

Este formato permite:

- detectar inicio de mensaje,
- conocer cuántos bytes forman el cuerpo,
- validar integridad con checksum,
- descartar tramas corruptas,
- resincronizar después de ruido o cortes parciales.

## Formato completo

```text
@LL:TTT:PAYLOAD:CC\n
```

Campos:

- `@`: carácter de inicio de trama.
- `LL`: longitud hexadecimal de `TTT:PAYLOAD`.
- `:`: separador.
- `TTT`: tipo de mensaje, siempre tres letras.
- `PAYLOAD`: contenido ASCII legible.
- `:`: separador antes del checksum.
- `CC`: checksum hexadecimal de 8 bits.
- `\n`: fin de trama.

Ejemplo:

```text
@08:CMD:ping:52\n
```

Desglose:

```text
@     inicio
08    longitud del cuerpo "CMD:ping"
:     separador
CMD   tipo de mensaje
:     separador interno entre tipo y payload
ping  payload
:     separador
52    checksum
\n    fin
```

## Qué se incluye en la longitud

La longitud `LL` cuenta solo el cuerpo:

```text
TTT:PAYLOAD
```

No cuenta:

- `@`,
- el campo `LL`,
- el separador después de `LL`,
- el separador antes del checksum,
- el checksum,
- `\n`.

Ejemplo con `ping`:

```text
CMD:ping
```

Cantidad de carácteres:

```text
C M D : p i n g
1 2 3 4 5 6 7 8
```

Longitud decimal: `8`.

Longitud hexadecimal de dos digitos: `08`.

Por eso el comienzo de la trama es:

```text
@08:CMD:ping
```

## Qué se incluye en el checksum

El checksum se calcula con XOR byte a byte sobre:

```text
LL:TTT:PAYLOAD
```

No incluye:

- `@`,
- el separador antes del checksum,
- el propio checksum,
- `\n`.

Para `ping`, el texto usado para checksum es:

```text
08:CMD:ping
```

El resultado es `0x52`, entonces la trama completa queda:

```text
@08:CMD:ping:52\n
```

## Algoritmo de checksum XOR

El checksum inicia en cero y se aplica XOR con cada byte ASCII:

```c
uint8_t checksum = 0U;

for (i = 0; i < length; i++) {
    checksum ^= data[i];
}
```

Propiedades útiles:

- es barato para un microcontrolador chico,
- detecta muchos errores simples,
- no requiere tablas,
- no es criptografico,
- no reemplaza CRC si se necesita alta robustez.

Ejemplo conceptual:

```text
data = "08:CMD:ping"
checksum = 0x00
checksum ^= '0'
checksum ^= '8'
checksum ^= ':'
...
resultado = 0x52
```

### Ejemplo numérico completo de XOR

Para calcular el checksum XOR de la cadena:

```text
08:CMD:ping
```

se toman los bytes ASCII de cada carácter, incluyendo los dos puntos `:`.

Representación ASCII en hexadecimal:

| Posición | Caracter | ASCII hex |
| --- | --- | --- |
| 1 | `0` | `30` |
| 2 | `8` | `38` |
| 3 | `:` | `3A` |
| 4 | `C` | `43` |
| 5 | `M` | `4D` |
| 6 | `D` | `44` |
| 7 | `:` | `3A` |
| 8 | `p` | `70` |
| 9 | `i` | `69` |
| 10 | `n` | `6E` |
| 11 | `g` | `67` |

El cálculo se hace acumulando XOR byte a byte:

```text
30 xor 38 = 08
08 xor 3A = 32
32 xor 43 = 71
71 xor 4D = 3C
3C xor 44 = 78
78 xor 3A = 42
42 xor 70 = 32
32 xor 69 = 5B
5B xor 6E = 35
35 xor 67 = 52
```

Resultado final:

```text
Hexadecimal: 0x52
Decimal: 82
Binario: 01010010
```

Por eso el campo `CC` de la trama `ping` es:

```text
52
```

## Tipos de mensaje

El campo `TTT` tiene exactamente tres letras mayusculas.

Tipos usados por la plantilla:

- `CMD`: comando desde PC/ROS 2 hacia la Blue Pill.
- `DAT`: dato o telemetria desde la Blue Pill.
- `EVT`: evento.
- `STS`: estado interno.
- `ACK`: confirmacion exitosa.
- `ERR`: error.

Ejemplos:

```text
@08:CMD:ping:52
@0E:CMD:led=toggle:7D
@0A:ACK:pong=1:22
@12:DAT:temp=29,seq=1:24
```

## Payload

El payload es texto ASCII imprimible. La plantilla recomienda formatos simples:

```text
clave=valor
accion=valor
clave1=valor1,clave2=valor2
```

Ejemplos:

```text
ping
status?
led=on
led=off
led=toggle
temp=29,seq=749
rx=7,ae=0,irq=112,pb=112,pm=7,pe=0,qd=0
```

Restricciones implementadas en firmware:

- longitud maxima definida por `PROTOCOL_MAX_PAYLOAD_LENGTH`,
- carácteres ASCII imprimibles entre `0x20` y `0x7E`,
- el payload no puede contener `@`,
- el tipo debe ser uno de los tipos conocidos.

## Generación de una trama

La generación se implementa en:

```text
firmware/protocol/protocol.c
ros2_bridge/bluepill_uart_bridge/protocol.py
```

La idea es la misma en C y Python.

Pasos:

1. Recibir un `type` y un `payload`.
2. Validar que el payload sea aceptable.
3. Convertir el tipo a texto de tres letras.
4. Construir el cuerpo `TTT:PAYLOAD`.
5. Calcular la longitud del cuerpo.
6. Construir el texto para checksum `LL:TTT:PAYLOAD`.
7. Calcular checksum XOR.
8. Construir la trama final `@LL:TTT:PAYLOAD:CC\n`.

Pseudocódigo:

```text
function encode(type, payload):
    type_text = type_to_text(type)
    body = type_text + ":" + payload
    length = len(body)
    checksum_input = hex2(length) + ":" + body
    checksum = xor(checksum_input)
    frame = "@" + hex2(length) + ":" + body + ":" + hex2(checksum) + "\n"
    return frame
```

## Ejemplo completo: generar `ping`

Entrada lógica:

```text
type = CMD
payload = ping
```

Cuerpo:

```text
CMD:ping
```

Longitud:

```text
len("CMD:ping") = 8 = 0x08
```

Entrada del checksum:

```text
08:CMD:ping
```

Checksum:

```text
0x52
```

Trama:

```text
@08:CMD:ping:52\n
```

## Ejemplo completo: generar `led=toggle`

Entrada lógica:

```text
type = CMD
payload = led=toggle
```

Cuerpo:

```text
CMD:led=toggle
```

Longitud:

```text
len("CMD:led=toggle") = 14 = 0x0E
```

Entrada del checksum:

```text
0E:CMD:led=toggle
```

Checksum:

```text
0x7D
```

Trama:

```text
@0E:CMD:led=toggle:7D\n
```

## Por qué el parser es incremental

UART entrega bytes, no mensajes completos. Una trama puede llegar:

- completa en una lectura,
- dividida en varias interrupciones,
- mezclada con ruido previo,
- cortada si hay errores,
- seguida inmediatamente por otra trama.

Por eso el parser no recibe strings completas. Recibe un byte por vez:

```c
parser_result_t parser_consume_byte(parser, byte, message);
```

La función puede devolver:

- `PARSER_RESULT_IN_PROGRESS`: todavía falta información.
- `PARSER_RESULT_MESSAGE_READY`: se completó una trama válida.
- `PARSER_RESULT_ERROR`: la trama parcial era inválida.

## FSM

La FSM está declarada en:

```text
firmware/protocol/parser.h
```

Estados:

```c
typedef enum {
    PARSER_STATE_WAIT_START = 0,
    PARSER_STATE_READ_LEN_HI,
    PARSER_STATE_READ_LEN_LO,
    PARSER_STATE_EXPECT_LEN_SEPARATOR,
    PARSER_STATE_READ_BODY,
    PARSER_STATE_EXPECT_CHECK_SEPARATOR,
    PARSER_STATE_READ_CHECK_HI,
    PARSER_STATE_READ_CHECK_LO,
    PARSER_STATE_EXPECT_END
} parser_state_t;
```

Cada estado representa exactamente qué byte espera el parser.

## Datos internos de la FSM

La estructura `parser_t` guarda el progreso:

```c
typedef struct {
    parser_state_t state;
    char length_field[3];
    char body[PROTOCOL_MAX_BODY_SIZE + 1U];
    char checksum_field[3];
    uint8_t expected_body_length;
    uint8_t body_index;
} parser_t;
```

Campos:

- `state`: estado actual.
- `length_field`: dos carácteres hexadecimales de longitud y terminador `\0`.
- `body`: buffer para `TTT:PAYLOAD`.
- `checksum_field`: dos carácteres hexadecimales del checksum y terminador `\0`.
- `expected_body_length`: longitud esperada del cuerpo ya convertida a numero.
- `body_index`: cuántos bytes del cuerpo ya se recibieron.

## Diagrama de estados

```text
WAIT_START
    |
    | '@'
    v
READ_LEN_HI
    |
    | hex
    v
READ_LEN_LO
    |
    | hex, longitud válida
    v
EXPECT_LEN_SEPARATOR
    |
    | ':'
    v
READ_BODY
    |
    | expected_body_length bytes
    v
EXPECT_CHECK_SEPARATOR
    |
    | ':'
    v
READ_CHECK_HI
    |
    | hex
    v
READ_CHECK_LO
    |
    | hex
    v
EXPECT_END
    |
    | '\n' + checksum OK + body OK
    v
MESSAGE_READY
    |
    v
WAIT_START
```

Ante un error en cualquier estado:

```text
parser_fail()
    |
    v
WAIT_START
```

Si el byte que causó el error es `@`, el parser lo reutiliza como comienzo de una trama nueva:

```text
ERROR con byte '@' -> READ_LEN_HI
```

Eso permite resincronizar rápido.

## Tabla de transiciones

| Estado | Byte esperado | Acción | Proximo estado |
| --- | --- | --- | --- |
| `WAIT_START` | `@` | iniciar trama | `READ_LEN_HI` |
| `WAIT_START` | otro | ignorar | `WAIT_START` |
| `READ_LEN_HI` | hex | guardar `LL[0]` | `READ_LEN_LO` |
| `READ_LEN_LO` | hex | guardar `LL[1]`, convertir longitud | `EXPECT_LEN_SEPARATOR` |
| `EXPECT_LEN_SEPARATOR` | `:` | confirmar separador | `READ_BODY` |
| `READ_BODY` | cualquier byte | acumular cuerpo | `READ_BODY` o `EXPECT_CHECK_SEPARATOR` |
| `EXPECT_CHECK_SEPARATOR` | `:` | confirmar separador | `READ_CHECK_HI` |
| `READ_CHECK_HI` | hex | guardar `CC[0]` | `READ_CHECK_LO` |
| `READ_CHECK_LO` | hex | guardar `CC[1]` | `EXPECT_END` |
| `EXPECT_END` | `\n` | validar checksum y cuerpo | `MESSAGE_READY` o error |

En los estados que esperan `hex`, solo se aceptan:

```text
0 1 2 3 4 5 6 7 8 9 A B C D E F
```

## Trazado byte a byte de `@08:CMD:ping:52\n`

Trama:

```text
@08:CMD:ping:52\n
```

Proceso:

| Byte | Estado antes | Acción | Estado después |
| --- | --- | --- | --- |
| `@` | `WAIT_START` | detecta inicio | `READ_LEN_HI` |
| `0` | `READ_LEN_HI` | guarda longitud alta | `READ_LEN_LO` |
| `8` | `READ_LEN_LO` | guarda longitud baja, calcula `0x08` | `EXPECT_LEN_SEPARATOR` |
| `:` | `EXPECT_LEN_SEPARATOR` | separador OK | `READ_BODY` |
| `C` | `READ_BODY` | body[0] | `READ_BODY` |
| `M` | `READ_BODY` | body[1] | `READ_BODY` |
| `D` | `READ_BODY` | body[2] | `READ_BODY` |
| `:` | `READ_BODY` | body[3] | `READ_BODY` |
| `p` | `READ_BODY` | body[4] | `READ_BODY` |
| `i` | `READ_BODY` | body[5] | `READ_BODY` |
| `n` | `READ_BODY` | body[6] | `READ_BODY` |
| `g` | `READ_BODY` | body[7], cuerpo completo | `EXPECT_CHECK_SEPARATOR` |
| `:` | `EXPECT_CHECK_SEPARATOR` | separador OK | `READ_CHECK_HI` |
| `5` | `READ_CHECK_HI` | guarda checksum alto | `READ_CHECK_LO` |
| `2` | `READ_CHECK_LO` | guarda checksum bajo | `EXPECT_END` |
| `\n` | `EXPECT_END` | valida checksum y body | `MESSAGE_READY` |

Al final se obtiene:

```text
type = CMD
payload = ping
```

## Validacion final en `EXPECT_END`

Cuando llega `\n`, todavía no se entrega el mensaje. Primero se valida:

1. Que el checksum recibido coincida.
2. Que el cuerpo tenga formato `TTT:PAYLOAD`.
3. Que `TTT` sea un tipo conocido.
4. Que el payload tenga carácteres válidos.

El checksum se recalcula sobre:

```text
LL:body
```

Para `ping`:

```text
08:CMD:ping
```

Si el checksum no coincide, se descarta la trama.

Si coincide, se llama a:

```c
protocol_decode_body(parser->body, parser->expected_body_length, message);
```

Esa función transforma:

```text
CMD:ping
```

en:

```c
message->type = PROTOCOL_TYPE_CMD;
message->payload = "ping";
message->payload_length = 4;
```

## Pseudocódigo del parser

```text
function consume(byte):
    if byte == '\r':
        return IN_PROGRESS

    switch state:
        WAIT_START:
            if byte == '@':
                state = READ_LEN_HI
            return IN_PROGRESS

        READ_LEN_HI:
            if byte is not hex:
                return fail(byte)
            length_field[0] = byte
            state = READ_LEN_LO
            return IN_PROGRESS

        READ_LEN_LO:
            if byte is not hex:
                return fail(byte)
            length_field[1] = byte
            expected_body_length = parse_hex(length_field)
            if expected_body_length is invalid:
                return fail(byte)
            state = EXPECT_LEN_SEPARATOR
            return IN_PROGRESS

        EXPECT_LEN_SEPARATOR:
            if byte != ':':
                return fail(byte)
            state = READ_BODY
            return IN_PROGRESS

        READ_BODY:
            body[body_index] = byte
            body_index += 1
            if body_index == expected_body_length:
                state = EXPECT_CHECK_SEPARATOR
            return IN_PROGRESS

        EXPECT_CHECK_SEPARATOR:
            if byte != ':':
                return fail(byte)
            state = READ_CHECK_HI
            return IN_PROGRESS

        READ_CHECK_HI:
            if byte is not hex:
                return fail(byte)
            checksum_field[0] = byte
            state = READ_CHECK_LO
            return IN_PROGRESS

        READ_CHECK_LO:
            if byte is not hex:
                return fail(byte)
            checksum_field[1] = byte
            state = EXPECT_END
            return IN_PROGRESS

        EXPECT_END:
            if byte != '\n':
                return fail(byte)
            if checksum is invalid:
                return fail(byte)
            if body is invalid:
                return fail(byte)
            reset()
            return MESSAGE_READY
```

## Manejo de `\r\n`

Algunos monitores serie envían fin de línea como:

```text
\r\n
```

El parser ignora `\r`:

```c
if (byte == '\r') {
    return PARSER_RESULT_IN_PROGRESS;
}
```

Por eso acepta tanto:

```text
@08:CMD:ping:52\n
```

como:

```text
@08:CMD:ping:52\r\n
```

## Resincronizacion ante ruido

Supongamos que llega ruido antes de una trama válida:

```text
xyz@08:CMD:ping:52\n
```

Mientras está en `WAIT_START`, el parser ignora:

```text
x
y
z
```

Cuando llega `@`, empieza una trama.

Supongamos una trama rota:

```text
@08?CMD:ping:52\n
```

En `EXPECT_LEN_SEPARATOR`, el parser esperaba `:` pero recibio `?`. Entonces:

1. descarta la trama parcial,
2. vuelve a `WAIT_START`,
3. espera el siguiente `@`.

Supongamos una trama rota seguida de otra válida:

```text
@08?CMD:ping:52\n@08:CMD:ping:52\n
```

El parser descarta la primera y recupera la segúnda.

## Por qué se valida longitud y checksum

La longitud protege contra:

- mensajes incompletos,
- separadores `:` dentro del payload interpretados incorrectamente,
- desalineación del parser.

El checksum protege contra:

- bytes corruptos,
- baudrate incorrecto,
- ruido eléctrico,
- cortes en la transmisión.

La combinación permite un parser simple y robusto para una UART educativa.

## Relación con FreeRTOS

En firmware, la UART y el parser están separados por colas:

```text
USART1 IRQ
  |
  | xQueueSendFromISR(byte)
  v
task_uart_rx
  |
  | xQueueSend(byte)
  v
task_parser
  |
  | parser_consume_byte()
  v
task_app
```

La ISR no parsea. Solo lee el byte del registro USART y lo manda a una cola.

Ventajas:

- la interrupción dura poco,
- el parser corre en contexto de tarea,
- la lógica de aplicación no bloquea la recepción UART,
- es más fácil depurar.

## Relación con ROS 2

El bridge Python implementa el mismo protocolo:

```text
ros2_bridge/bluepill_uart_bridge/protocol.py
```

Cuando se publica:

```bash
ros2 topic pub --once /bridge/tx_cmd std_msgs/msg/String "{data: ping}"
```

el bridge hace:

```text
ProtocolMessage(MessageType.CMD, "ping")
encode_frame(...)
```

y envía por serial:

```text
@08:CMD:ping:52\n
```

Cuando recibe una trama desde la Blue Pill, usa su parser incremental y publica según el tipo:

- `DAT` -> `/bridge/data`
- `STS` -> `/bridge/status`
- `ACK` -> `/bridge/ack`
- `ERR` -> `/bridge/error`
- `EVT` -> `/bridge/event`

## Ejemplo extremo a extremo

Comando desde ROS 2:

```bash
ros2 topic pub --once /bridge/tx_cmd std_msgs/msg/String "{data: ping}"
```

Trama enviada por UART:

```text
@08:CMD:ping:52\n
```

La Blue Pill parsea:

```text
type = CMD
payload = ping
```

La app responde:

```text
type = ACK
payload = pong=1
```

Trama de respuesta:

```text
@0A:ACK:pong=1:22\n
```

ROS 2 publica:

```text
/bridge/ack -> "pong=1"
```

## Errores comunes

### Longitud incorrecta

Trama incorrecta:

```text
@09:CMD:ping:53\n
```

`CMD:ping` mide `08`, no `09`. El parser espera un byte más de cuerpo y se desalineará.

### Checksum incorrecto

Trama incorrecta:

```text
@08:CMD:ping:00\n
```

La longitud y formato son correctos, pero el checksum no coincide. El parser descarta la trama.

### Falta `\n`

Trama incompleta:

```text
@08:CMD:ping:52
```

El parser queda esperando `\n`. No entrega `MESSAGE_READY`.

### Tipo desconocido

Trama con tipo invalido:

```text
@08:XYZ:ping:50\n
```

Aunque el checksum fuese correcto, `XYZ` no es un tipo soportado. `protocol_decode_body()` rechaza el mensaje.

## Recomendaciones para extender el protocolo

- Mantener tipos de tres letras.
- Agregar comandos nuevos como payloads `CMD`, no inventar tipos si no hace falta.
- Usar payloads cortos y legibles.
- Documentar cada payload nuevo.
- Evitar datos binarios dentro del payload.
- Si se necesitan datos complejos, usar pares `clave=valor` separados por coma.
- Mantener el parser byte-a-byte.
- No usar `sscanf` en la ruta critica del parser.

Ejemplos de comandos recomendados:

```text
servo=90
buzzer=on
mode=auto
threshold=35
fan=off
```

Ejemplos de telemetria recomendada:

```text
temp=29,hum=55
button=pressed
distance=123
mode=auto,state=idle
```

## Archivos relevantes

- `firmware/protocol/protocol.h`: tipos, constantes y estructura `protocol_message_t`.
- `firmware/protocol/protocol.c`: generación de tramas y decodificación del cuerpo.
- `firmware/protocol/parser.h`: estados de la FSM y estructura `parser_t`.
- `firmware/protocol/parser.c`: implementación de la FSM byte-a-byte.
- `ros2_bridge/bluepill_uart_bridge/protocol.py`: implementación equivalente en Python.
- `ros2_bridge/bluepill_uart_bridge/serial_bridge_node.py`: puente entre topicos ROS 2 y UART.
