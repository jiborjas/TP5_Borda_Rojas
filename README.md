# TP5 UART - Borda Rojas

Entrega del TP5: protocolo serie con framing y checksum sobre UART en STM32F103 Blue Pill.

El firmware implementa el protocolo:

```text
@LL:TTT:PAYLOAD:CC\n
```

con checksum XOR sobre `LL:TTT:PAYLOAD`, parser incremental byte a byte, comandos de LED, `status?`, errores y resincronizacion.

## Estado

- Etapa 1 completa: framing, checksum, validacion de tramas y tests host.
- Etapa 2 completa: parser incremental y comandos `ping`, `led=on`, `led=off`, `led=toggle`.
- Etapa 3 completa: `status?`, contadores, errores y resincronizacion.

## Estructura

```text
.
├── firmware/        Firmware FreeRTOS + libopencm3 para Blue Pill
├── tests/host/      Tests ejecutables en PC
├── evidencia/       Evidencia por etapa con tramas crudas
├── docs/            Documentacion de referencia de la base de catedra
├── scripts/         Herramientas auxiliares UART
├── INFORME.md       Informe, evidencias y respuestas obligatorias
├── TP5 UART.pdf     Consigna
└── Makefile         Comandos delegados desde la raiz
```

## Verificacion rapida

Desde la raiz del repositorio:

```bash
make firmware-test
make
```

Equivalente manual:

```bash
cd tests/host
make clean test

cd ../../firmware
make
```

Resultado esperado de tests:

```text
All protocol tests passed
All parser tests passed
All app tests passed
```

## Flasheo

Con la Blue Pill conectada por ST-Link:

```bash
make flash
```

Ese comando delega en `firmware/Makefile` y usa OpenOCD:

```bash
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg -c "program bin/main.elf verify reset exit"
```

## Conexion UART

Configuracion:

- USART1
- 115200 baud
- 8N1
- sin control de flujo

Cableado:

```text
Blue Pill PA9  (TX) -> RX del adaptador USB-UART
Blue Pill PA10 (RX) -> TX del adaptador USB-UART
Blue Pill GND       -> GND del adaptador USB-UART
```

Usar adaptador USB-UART TTL de 3.3 V. No conectar niveles RS232 ni TX de 5 V directo a PA10.

## Comandos de prueba

Tramas para enviar desde monitor serie:

```text
@08:CMD:ping:52
@0A:CMD:led=on:6A
@0B:CMD:led=off:07
@0E:CMD:led=toggle:7D
@0B:CMD:status?:13
```

Respuestas esperadas:

```text
@0A:ACK:pong=1:22
@0A:ACK:cmd=ok:6B
@XX:STS:rx=N,ae=N,irq=N,pb=N,pm=N,pe=N,qd=N:YY
@14:ERR:code=unknown_cmd:2D
```

El firmware transmite `\r\n` hacia UART para que los monitores serie muestren las lineas alineadas. El protocolo interno sigue usando `\n`, y el parser ignora `\r`.

## Evidencia

- `evidencia/etapa1/`: framing, checksums y validacion de trama.
- `evidencia/etapa2/`: parser incremental, comandos LED y ruido antes de trama valida.
- `evidencia/etapa3/`: `status?`, comando desconocido y resincronizacion con `@0@08:CMD:ping:52\n`.

`INFORME.md` contiene el desarrollo completo, las respuestas obligatorias y el checklist final.

## Diagrama de flujos
![Diagrama de flujos](SE_TP5_Flujos.png)