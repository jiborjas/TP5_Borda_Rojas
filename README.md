# TP5_Borda_Rojas

Trabajo practico 5 de Sistemas Embebidos: protocolo serie con framing y
checksum XOR sobre UART en STM32F103 Blue Pill.

El proyecto es autocontenido. Todo lo necesario para compilar el firmware esta
dentro de `TP5_Borda_Rojas/firmware`, incluidas las third parties:

- `firmware/third_party/libopencm3`
- `firmware/third_party/FreeRTOS-Kernel`
- `firmware/third_party/common/linker.ld`

No hay dependencias a rutas externas del workspace.

## Alcance

El firmware implementa recepcion por UART con parser incremental, comandos de
aplicacion sobre la Blue Pill, telemetria periodica y mensajes de estado para
validacion en PC.

## Estructura

- `firmware/protocol`: codificacion de tramas, checksum, validacion y parser FSM.
- `firmware/app`: comandos `ping`, `led=on`, `led=off`, `led=toggle`, `status?`.
- `firmware/drivers`: USART1 por PA9/PA10 a 115200 8N1.
- `firmware/platform`: clock y setup base de STM32F103.
- `tests/host`: tests de PC para protocolo y parser, sin FreeRTOS ni hardware.
- `tools`: script de prueba serie contra una placa conectada.
- `evidencia`: salidas de tests y tramas de referencia.

## Protocolo

Formato de trama:

```text
@LL:TTT:PAYLOAD:CC\n
```

`LL` es la longitud hexadecimal de `TTT:PAYLOAD`. `CC` es el XOR byte a byte
calculado sobre `LL:TTT:PAYLOAD`, sin incluir `@`, el separador previo al
checksum ni `\n`.

Tramas principales:

```text
@08:CMD:ping:52\n        -> @0A:ACK:pong=1:22\n
@0A:CMD:led=on:6A\n      -> @0A:ACK:cmd=ok:6B\n
@0B:CMD:led=off:07\n     -> @0A:ACK:cmd=ok:6B\n
@0E:CMD:led=toggle:7D\n  -> @0A:ACK:cmd=ok:6B\n
@0B:CMD:status?:13\n     -> @XX:STS:rx=N,ae=N,irq=N,pb=N,pm=N,pe=N,qd=N:YY\n
```

## Compilacion y pruebas

Tests de host:

```sh
cd TP5_Borda_Rojas/tests/host
make run
```

Firmware:

```sh
cd TP5_Borda_Rojas/firmware
make
make size
make test-host
```

Flasheo con OpenOCD:

```sh
cd TP5_Borda_Rojas/firmware
make flash
```

Otros targets utiles del Makefile:

```sh
make clean      # borra bin/
make rebuild    # limpia y recompila
make hex        # genera/actualiza .hex
make bin        # genera/actualiza .bin
make list       # genera bin/tp5_borda_rojas.lst
make erase      # borrado masivo via OpenOCD
make openocd    # servidor OpenOCD
make gdb        # conexion GDB a localhost:3333
```

## Conexionado

- Blue Pill PA9 TX -> RX del adaptador USB-UART.
- Blue Pill PA10 RX -> TX del adaptador USB-UART.
- GND Blue Pill -> GND del adaptador.
- Monitor serie: 115200 baud, 8N1, sin control de flujo.

El LED integrado esta en PC13 y es activo en bajo: `led=on` limpia el pin,
`led=off` lo pone en alto.

## Referencias

La implementacion sigue la consigna `TPs Sistemas Embebidos/TP5 UART.pdf` y se
apoya en el apunte 13 de protocolo serie, el apunte 08 de perifericos STM32F103
y la bibliografia RM0008/datasheet STM32F103C8T6.
