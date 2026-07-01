# Protocolo UART ASCII Estructurado

Este archivo resume el formato. Para una explicación didáctica completa de generación, checksum, parser incremental y FSM, ver:

- `docs/FRAME_GENERATION_AND_PARSING.md`

## Formato

```text
@LL:TTT:PAYLOAD:CC\n
```

## Campos

- `@`: inicio de trama.
- `LL`: longitud en hexadecimal de `TTT:PAYLOAD`.
- `TTT`: tipo de mensaje de tres letras.
- `PAYLOAD`: contenido ASCII legible.
- `CC`: checksum XOR sobre `LL:TTT:PAYLOAD`.
- `\n`: fin de trama.

## Motivos de este diseno

- Legible desde un monitor serie.
- Formaliza el framing.
- Obliga a validar longitud y checksum.
- Se parsea byte a byte sin depender de strings completas ni buffers enormes.

## Tipos base

- `CMD`
- `DAT`
- `EVT`
- `STS`
- `ACK`
- `ERR`

## Payloads sugeridos

Payloads de estilo `clave=valor`, `accion=valor` o listas cortas separadas por `,`.

Ejemplos:

- `led=on`
- `temp=23.4`
- `button=pressed`
- `mode=auto`

## Parser incremental

El parser procesa un byte por vez y nunca asume que la trama llega de una sola vez. Eso permite integrarlo facilmente con:

- interrupciones UART,
- colas FreeRTOS,
- buffers circulares,
- streams con ruido o cortes parciales.

## Resincronizacion

Ante cualquier error:

- se descarta la trama parcial,
- se vuelve a `WAIT_START`,
- si el byte actual es `@`, se aprovecha como nuevo inicio valido.
