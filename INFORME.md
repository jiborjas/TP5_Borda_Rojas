# Informe TP5 - Borda Rojas

## Resumen

El firmware implementa el protocolo `@LL:TTT:PAYLOAD:CC\n` sobre USART1 de la
Blue Pill. La recepcion entra por interrupcion, se desacopla con colas de
FreeRTOS, pasa por un parser incremental y llega a la aplicacion como mensajes
validados. La transmision toma mensajes logicos y los serializa con longitud y
checksum correctos.

## Alcance

La entrega cubre framing, checksum, parser incremental, despachos de comando,
telemetria periodica y mensajes de estado para validacion desde PC.

## Implementacion

Etapa 1:

- `protocol_checksum()` calcula XOR sobre `LL:TTT:PAYLOAD`.
- `hex_char_to_nibble()` acepta `0-9` y `A-F`.
- `protocol_encode()` y `protocol_encode_frame()` generan tramas completas.
- `protocol_validate()` verifica longitud declarada, formato, tipo y checksum.

Etapa 2:

- `parser_consume_byte()` implementa los 9 estados pedidos por la consigna.
- El parser consume un byte por llamada, ignora `\r`, entrega mensajes al llegar
  a `\n` con checksum valido y se resincroniza reutilizando `@` si aparece en un
  punto de error.
- `app_handle_message()` despacha `ping`, `led=on`, `led=off` y `led=toggle`.

Etapa 3:

- `status?` responde con `STS:rx=N,ae=N,irq=N,pb=N,pm=N,pe=N,qd=N`.
- Se cuentan errores de aplicacion, bytes procesados, mensajes validados,
  errores de parser, bytes de ISR y descartes de cola.
- Comandos desconocidos devuelven `ERR:code=unknown_cmd`.
- Tipos distintos de `CMD` devuelven `ERR:code=unexpected_type`.

## Evidencia

La evidencia de PC esta en:

- `evidencia/etapa1/test_protocol.txt`
- `evidencia/etapa2/test_parser.txt`
- `evidencia/etapa3/tramas_referencia.txt`

Tambien se puede regenerar con:

```sh
cd tests/host
make run
```

Casos verificados por tests:

- `@08:CMD:ping:52\n`
- `@0A:CMD:led=on:6A\n`
- `@0B:CMD:led=off:07\n`
- `@0E:CMD:led=toggle:7D\n`
- `@0B:CMD:status?:13\n`
- checksum valido e invalido
- ruido antes de una trama valida
- `\r\n`
- resincronizacion con `@0@08:CMD:ping:52\n`
- dos tramas pegadas

## Etapa 1

1. `LL` cuenta solo `TTT:PAYLOAD` porque ese es el cuerpo que el receptor debe
   acumular antes de esperar el checksum. Si incluyera `@`, separadores externos,
   `CC` o `\n`, el parser tendria que conocer campos que todavia no leyo para
   saber cuando termina el cuerpo, y se complicaria la recuperacion ante ruido.

2. Si se calcula el checksum incluyendo `@`, el valor cambia para `ping`.
   `08:CMD:ping` da `0x52`; `@08:CMD:ping` da `0x12` porque `0x52 XOR 0x40 =
   0x12`. No coincide con el recalculo que usa el bridge.

3. `protocol_validate()` busca el ultimo `:` porque el payload puede contener
   `:`. Si buscara el primero, separaria despues de `LL` y confundiria casi toda
   la trama con el checksum. El ultimo separador es el unico que delimita `CC`.

## Etapa 2

1. El parser debe ser incremental porque UART entrega un flujo de bytes, no
   mensajes atomicos. Leer una linea completa falla si llegan dos tramas pegadas,
   si aparece ruido antes de `@`, si una trama se corta a mitad de recepcion o si
   el buffer se llena antes de encontrar `\n`.

2. Si `\r` no se ignorara, una terminal que envia CRLF produciria error en
   `EXPECT_END`: el parser ya leyo `CC` y espera `\n`, pero recibe `\r`.

3. Dos tramas pegadas funcionan porque al validar la primera en `EXPECT_END` el
   parser entrega el mensaje y vuelve a `WAIT_START`. El byte siguiente es el `@`
   de la segunda trama, por lo que recorre `READ_LEN_HI`, `READ_LEN_LO`,
   `EXPECT_LEN_SEPARATOR`, `READ_BODY`, `EXPECT_CHECK_SEPARATOR`,
   `READ_CHECK_HI`, `READ_CHECK_LO` y `EXPECT_END` sin necesitar delays.

## Etapa 3

1. Los contadores internos muestran comportamiento de software: mensajes
   validos, errores de aplicacion, errores del parser y descartes de cola. Los
   registros USART solo muestran estado instantaneo del periferico; no dicen
   cuantos comandos se procesaron ni cuantas tramas se descartaron por checksum.

2. En `@0@08:CMD:ping:52\n`, sin reutilizar el segundo `@`, el parser entregaria
   cero mensajes. Tras `@0`, el segundo `@` causa error en `READ_LEN_LO`; si se
   descarta, los bytes siguientes `08:CMD:ping:52\n` llegan estando en
   `WAIT_START` pero ya no tienen `@`, asi que se ignoran.

3. Si el bridge envia `@0B:CMD:status?:13\n` y recibe `STS` con
   `rx=5,ae=2,...`, publica en el topico de estado (`bridge/status`) el payload
   recibido. Es `STS` y no `ACK` porque no solo confirma el comando: transporta
   estado diagnostico del firmware.

## Cierre

1. XOR detecta cambios de un bit y muchos errores simples, pero no todos. Si dos
   bytes se intercambian, el XOR no cambia porque la operacion es conmutativa.
   Para mayor robustez usaria CRC-8 o CRC-16 manteniendo la misma estructura de
   trama y reemplazando el campo `CC`.

2. A 115200 baud 8N1 cada caracter usa 10 bits. Una trama de 64 caracteres usa
   640 bits, por lo que tarda `640 / 115200 = 0,00556 s`, unos 5,56 ms. Si `LL`
   dice `FF`, este parser lo rechaza al decodificar la longitud porque supera
   `PROTOCOL_MAX_BODY_SIZE`; no queda esperando. Si un parser aceptara `FF`,
   esperaria hasta juntar 255 bytes o hasta que aparezca un error de formato.

3. Para `###@08:CMD:ping:52\n`, los tres `#` se ignoran en `WAIT_START`; `@`
   pasa a `READ_LEN_HI`; `0` a `READ_LEN_LO`; `8` fija longitud; `:` pasa a
   `READ_BODY`; se lee `CMD:ping`; `:` pasa a checksum; `5`, `2` completan `CC`;
   `\n` valida y entrega un mensaje `CMD:ping`. A diferencia de
   `@0@08:CMD:ping:52\n`, no hay una trama parcial rota antes de la valida.

4. La Blue Pill trabaja a 3,3 V. Conectar PA10 directo a un TX de 5 V no es una
   practica segura aunque algunos pines sean tolerantes en ciertas condiciones.
   Conviene usar adaptador USB-UART de 3,3 V o un conversor de nivel/divisor
   resistivo hacia RX del micro. Esto afecta al hardware electrico, no al
   protocolo.

## Checklist de entrega

- Firmware autocontenido con third parties copiadas.
- Tests host incluidos.
- Parser incremental con resincronizacion.
- Comandos y respuestas de la consigna implementados.
- Contadores de estado implementados.
- Respuestas teoricas incluidas en este informe.
