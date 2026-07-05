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

## Preguntas obligatorias

### Etapa 1

1. `LL` cuenta solamente `TTT:PAYLOAD` porque ese es el cuerpo que el receptor necesita acumular antes de buscar el checksum. Si incluyera `@`, separadores externos, `CC` y `\n`, el parser tendria que conocer el largo de campos que justamente sirven para encontrar los limites de la trama. No aporta informacion y complica el parsing.

2. Si se incluye `@` en el XOR, el checksum cambia porque `@` vale `0x40`. Para `08:CMD:ping`, el checksum correcto es `0x52`. Si se calcula sobre `@08:CMD:ping`, da `0x12` porque `0x52 ^ 0x40 = 0x12`. No funcionaria con el bridge de la catedra, porque el bridge espera el checksum definido por protocolo, sin la arroba.

3. No conviene buscar el primer `:` para separar checksum, porque la trama tiene varios separadores. Ademas, el payload podria contener `:` si se extendiera el protocolo o si se enviara texto con separadores. Buscar el ultimo `:` permite separar correctamente `CC` del resto de la trama.

### Etapa 2

1. El parser debe ser incremental porque la UART entrega bytes, no mensajes completos. Leer una linea completa fallaria si llega ruido antes de `@`, si se pierde el `\n`, si llegan dos tramas pegadas, o si el buffer de linea queda esperando indefinidamente por una trama incompleta.

2. Si `\r` no se ignorara, en una terminal que manda `\r\n` el parser llegaria a `EXPECT_END` esperando `\n`, recibiria `\r` y marcaria error. Ignorarlo permite aceptar tanto `\n` como cierre real y `\r\n` como formato tipico de monitor serie.

3. Si llegan `@08:CMD:ping:52\n@0A:CMD:led=on:6A\n`, al terminar la primera el parser vuelve a `WAIT_START`. El siguiente byte ya es `@`, entonces recorre: `WAIT_START -> READ_LEN_HI -> READ_LEN_LO -> EXPECT_LEN_SEPARATOR -> READ_BODY -> EXPECT_CHECK_SEPARATOR -> READ_CHECK_HI -> READ_CHECK_LO -> EXPECT_END`. No hace falta delay porque cada byte se procesa en orden y el estado se reinicia al entregar la primera trama.

### Etapa 3

1. Conviene reportar contadores porque describen el comportamiento del software, no solo el estado instantaneo del hardware. Un registro USART puede decir si hay un byte pendiente o si hubo overrun, pero no dice cuantos mensajes validos se parsearon, cuantos errores de aplicacion hubo, cuantos bytes se descartaron por cola llena ni si la aplicacion esta recibiendo comandos.

2. En `@0@08:CMD:ping:52\n`, sin reutilizar la segunda `@`, el parser haria: primer `@` inicia, `0` se lee como primer digito de longitud, segundo `@` causa error en `READ_LEN_LO` y se descarta. Luego seguirian `0`, `8`, `:`, etc. en `WAIT_START`, pero como ya se perdio la arroba real, no se detecta nueva trama. Entregaria 0 mensajes. Con reutilizacion, entrega 1 mensaje: `CMD:ping`.

3. Si el bridge recibe una trama `STS`, publica en el topico de estado, normalmente `bridge/status`, con el payload `rx=5,ae=2,...`. Es `STS` y no `ACK` porque no confirma solamente que el comando fue recibido: transporta informacion de diagnostico del firmware.

### Cierre

1. El XOR detecta muchos errores simples, por ejemplo un bit cambiado o un byte cambiado. No detecta todos los errores: si se alteran dos bytes con cambios que se cancelan en XOR, el checksum queda igual. Si se intercambian dos bytes del cuerpo sin modificar sus valores, XOR no lo detecta porque XOR es conmutativo. Para mayor robustez usaria CRC-8 o CRC-16 manteniendo la misma estructura textual del campo `CC` o ampliandolo si hiciera falta.

2. A 115200 baud 8N1, cada caracter usa 10 bits: 1 start, 8 datos, 1 stop. Una trama de 64 caracteres usa 640 bits. Tiempo = 640 / 115200 = 0,00556 s = 5,56 ms. Si `LL` dice `FF`, este firmware lo rechaza porque supera `PROTOCOL_MAX_BODY_SIZE`; vuelve a esperar una nueva trama. Si un parser aceptara `FF`, quedaria esperando hasta completar 255 bytes o hasta que aparezca un error/resincronizacion. No deberia ocurrir si el emisor respeta el maximo de trama.

3. Para `###@08:CMD:ping:52\n`: en `WAIT_START` se ignoran los tres `#`; con `@` pasa a `READ_LEN_HI`; `0` a `READ_LEN_LO`; `8` a `EXPECT_LEN_SEPARATOR`; `:` a `READ_BODY`; consume `CMD:ping`; `:` a `READ_CHECK_HI`; `5` a `READ_CHECK_LO`; `2` a `EXPECT_END`; `\n` valida y entrega 1 mensaje. La diferencia con `@0@08...` es que ahi la basura empieza como una trama parcial y requiere resincronizar reutilizando la segunda arroba.

4. No es seguro conectar directamente PA10 a un TX de adaptador USB-UART de 5 V: el STM32F103 trabaja a 3,3 V y se puede danar o estresar el pin. Usaria un adaptador TTL de 3,3 V o un conversor de nivel/resistencias adecuadas. Esto afecta al hardware electrico, no al protocolo: las tramas y checksums son iguales si los niveles llegan correctamente.

## Checklist de entrega

- Codigo de framing/checksum completo.
- Parser incremental de 9 estados completo.
- `app_handle_message()` con `ping`, `led=on`, `led=off`, `led=toggle`, `status?`, errores y contadores.
- Tests host reproducibles.
- Evidencia textual de tramas crudas para Etapas 1, 2 y 3.

## Diagrama de flujos
![Diagrama de flujos](SE_TP5_Flujos.png)