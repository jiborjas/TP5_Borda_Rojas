# TP5 UART - Informe

## Etapa 1: framing y checksum

### Objetivo

Implementar las funciones basicas del protocolo serie:

- calcular el checksum XOR de 8 bits;
- construir tramas con formato `@LL:TTT:PAYLOAD:CC\n`;
- validar tramas recibidas, incluyendo longitud y checksum;
- decodificar el cuerpo `TTT:PAYLOAD`;
- convertir caracteres hexadecimales a valores numericos.

Esta etapa todavia no depende de la UART ni de FreeRTOS. Se puede verificar en la PC con tests host porque las funciones trabajan con strings y buffers en memoria.

### Criterio usado para validar

La consigna define que `LL` es la longitud hexadecimal del cuerpo `TTT:PAYLOAD`, y que `CC` se calcula con XOR byte a byte sobre `LL:TTT:PAYLOAD`.

Por ejemplo, para `CMD:ping`:

- cuerpo: `CMD:ping`;
- longitud del cuerpo: 8 bytes, por eso `LL = 08`;
- texto usado para checksum: `08:CMD:ping`;
- checksum esperado: `52`;
- trama completa esperada: `@08:CMD:ping:52\n`.

La evidencia compara las tramas generadas por `protocol_encode_frame()` contra los valores de referencia de la consigna. Eso prueba simultaneamente longitud, tipo, payload, separadores, checksum y terminador de linea.

### Evidencia generada

Archivos:

- `tests/host/test_protocol.c`: prueba automatica de checksum, encode, decode y conversion hexadecimal.
- `evidencia/etapa1/tramas_referencia.txt`: tramas de referencia usadas para validar.
- `evidencia/etapa1/test_protocol.txt`: salida esperada de la prueba host.

Comando ejecutado:

```bash
cd tests/host
make test
```

Resultado:

```text
All protocol tests passed
```

Tambien se compilo el firmware completo desde `firmware/` con `make`. La compilacion genero `bin/main.elf`, `bin/main.hex` y `bin/main.bin`, confirmando que la implementacion de Etapa 1 integra con el proyecto embebido.

### Cobertura de la evidencia minima

La evidencia cubre los puntos pedidos por la consigna para Etapa 1:

- `protocol_compute_checksum()` genera los checksums de referencia `52`, `6A`, `7D` y `6B`.
- `protocol_encode_frame()` genera tramas completas para `ping`, `led=on`, `led=off`, `led=toggle`, `status?`, `pong=1`, `cmd=ok` y `code=unknown_cmd`.
- `protocol_validate_frame()` acepta `@08:CMD:ping:52\n` y rechaza una version con checksum alterado `@08:CMD:ping:53\n`.
- `protocol_decode_body()` acepta `CMD:ping` y rechaza cuerpos con separador o tipo invalido.
- `hex_char_to_nibble()` convierte digitos hexadecimales validos y rechaza caracteres no hexadecimales.

### Limitaciones

Esta etapa no prueba recepcion byte a byte ni resincronizacion ante ruido. Esos comportamientos pertenecen a la Etapa 2, donde se implementa `parser_consume_byte()`.

## Etapa 2: parser incremental y comandos LED

### Objetivo

Implementar una maquina de estados finitos que consuma bytes UART de a uno y entregue mensajes completos solo cuando la trama sea valida. Con esos mensajes, la aplicacion despacha:

- `ping` -> `ACK:pong=1`;
- `led=on` -> enciende LED PC13 y responde `ACK:cmd=ok`;
- `led=off` -> apaga LED PC13 y responde `ACK:cmd=ok`;
- `led=toggle` -> conmuta LED PC13 y responde `ACK:cmd=ok`.

### Implementacion

El parser esta en `firmware/protocol/parser.c` y usa los 9 estados pedidos:

```text
WAIT_START -> READ_LEN_HI -> READ_LEN_LO -> EXPECT_LEN_SEPARATOR
           -> READ_BODY -> EXPECT_CHECK_SEPARATOR
           -> READ_CHECK_HI -> READ_CHECK_LO -> EXPECT_END
```

La aplicacion esta en `firmware/app/app.c`. No escribe directamente en UART: arma `protocol_message_t` y lo manda a la cola TX. La tarea `task_uart_tx()` codifica el mensaje con `protocol_encode_frame()` y recien ahi envia los bytes.

### Evidencia

Archivos:

- `tests/host/test_parser.c`: verifica parser byte a byte.
- `tests/host/test_app.c`: verifica comandos de aplicacion con stubs de FreeRTOS.
- `evidencia/etapa2/monitor_serie_host.txt`: intercambio esperado con tramas crudas.
- `evidencia/etapa2/test_parser_app.txt`: salida de tests host.

Casos cubiertos:

- `@08:CMD:ping:52\n` entrega `CMD:ping`.
- `xyz@08:CMD:ping:52\n` ignora basura y entrega `CMD:ping`.
- `@08:CMD:ping:53\n` se rechaza por checksum corrupto.
- `@08:CMD:ping:52\n@0A:CMD:led=on:6A\n` entrega dos mensajes pegados.
- `@08:CMD:ping:52\r\n` funciona porque `\r` se ignora.
- `ping`, `led=on` y errores se prueban en `app_handle_message()`.


## Etapa 3: estado, errores y resincronizacion

### Objetivo

Completar el firmware con:

- comando `status?`;
- respuesta `ERR:code=unknown_cmd` para comandos desconocidos;
- respuesta `ERR:code=unexpected_type` para tipos entrantes distintos de `CMD`;
- contadores de diagnostico;
- resincronizacion cuando aparece ruido o una arroba dentro de una trama rota.

### Implementacion

`app_build_status_message()` construye el payload:

```text
rx=N,ae=N,irq=N,pb=N,pm=N,pe=N,qd=N
```

Significado:

- `rx`: mensajes que llegaron a la aplicacion;
- `ae`: errores de aplicacion;
- `irq`: bytes recibidos por la ISR UART;
- `pb`: bytes procesados por el parser;
- `pm`: mensajes validos entregados por el parser;
- `pe`: errores detectados por el parser;
- `qd`: bytes descartados porque la cola de RX estaba llena.

La resincronizacion esta en `parser_error()`: si el byte que causa el error es `@`, se reinicia el parser pero se conserva esa arroba como inicio de una nueva trama.

### Evidencia

Archivos:

- `tests/host/test_parser.c`: cubre `@0@08:CMD:ping:52\n`.
- `tests/host/test_app.c`: cubre `status?`, comando desconocido y tipo inesperado.
- `evidencia/etapa3/monitor_serie_host.txt`: tramas crudas esperadas.
- `evidencia/etapa3/test_status_resync.txt`: salida de tests host relevante.

La evidencia host usa contadores simulados para que el resultado sea reproducible:

```text
TX @0B:CMD:status?:13\n
RX @28:STS:rx=3,ae=1,irq=7,pb=11,pm=5,pe=2,qd=1:0B\n
```

Limitacion practica: el payload maximo es 48 bytes. Con contadores chicos entra sin problema. Si los contadores crecieran a muchos digitos, el string podria truncarse; para una demo de TP se recomienda reiniciar la placa antes de tomar evidencia.

## Verificacion final

Comandos ejecutados:

```bash
cd tests/host
make clean test
```

Resultado:

```text
All protocol tests passed
All parser tests passed
All app tests passed
```

```bash
cd firmware
make
```

Resultado: se generan `bin/main.elf`, `bin/main.hex` y `bin/main.bin`.

## Checklist de entrega

- Codigo de framing/checksum completo.
- Parser incremental de 9 estados completo.
- `app_handle_message()` con `ping`, `led=on`, `led=off`, `led=toggle`, `status?`, errores y contadores.
- Tests host reproducibles.
- Evidencia textual de tramas crudas para Etapas 1, 2 y 3.

## Diagrama de flujos
![Diagrama de flujos](SE_TP5_Flujos.png)