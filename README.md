# bluepill-freertos-ros2-alumnos

Versión para alumnos de **Sistemas Embebidos**.

Esta base ya trae:

- inicialización de la **Blue Pill STM32F103C8T6**,
- estructura modular del firmware,
- **FreeRTOS** con tareas separadas,
- drivers de UART,
- aplicación de ejemplo con sensores/actuadores simulados,
- paquete base de **ROS 2**,
- documentación de instalación, uso y debugging.

La parte que **NO** está resuelta a propósito es la implementación del protocolo UART entre la Blue Pill y ROS 2.


## Qué tienen que hacer ustedes

Implementar la comunicación UART completa para que el sistema pueda:

1. codificar tramas en la Blue Pill,
2. parsear bytes entrantes en la Blue Pill,
3. codificar tramas desde el bridge ROS 2,
4. parsear tramas recibidas desde la placa en ROS 2.

Los archivos clave que quedaron como scaffold son:

- `firmware/protocol/protocol.c`
- `firmware/protocol/parser.c`
- `ros2_bridge/bluepill_uart_bridge/protocol.py`
- `ros2_bridge/bluepill_uart_bridge/serial_bridge_node.py`

## Objetivo de la práctica

La PC corre ROS 2 y se comunica con la Blue Pill mediante un adaptador USB-UART.

Flujo general:

```text
ROS 2 PC
  |
  | /bridge/tx_cmd, /bridge/tx_frame
  | /bridge/rx_raw, /bridge/data, /bridge/status, /bridge/ack, /bridge/error
  |
bluepill_uart_bridge
  |
  | USB-UART 115200 8N1
  |
Blue Pill STM32F103C8T6 + FreeRTOS
  |
Sensores y actuadores del proyecto
```

Cuando completen el protocolo, deberían poder:

- mandar comandos ROS 2 hacia la Blue Pill,
- recibir telemetría y estado desde la Blue Pill,
- validar integridad de mensajes,
- demostrar una integración extremo a extremo estable.

## Recorrido sugerido

1. Leer `ENUNCIADO.md`.
2. Leer `docs/PROTOCOL.md`.
3. Leer `docs/FRAME_GENERATION_AND_PARSING.md`.
4. Revisar `docs/ARCHITECTURE.md`.
5. Hacer el setup con `docs/SETUP_AND_SMOKETEST.md`.
6. Implementar firmware del protocolo.
7. Implementar bridge ROS 2.
8. Probar con los comandos de este README.

## Estructura del repositorio

```text
.
├── ENUNCIADO.md
├── README.md
├── docs/
│   ├── ARCHITECTURE.md
│   ├── FRAME_GENERATION_AND_PARSING.md
│   ├── PRACTICA_UART_ROS2.md
│   ├── PROTOCOL.md
│   ├── SETUP_AND_SMOKETEST.md
│   └── UART_TROUBLESHOOTING.md
├── firmware/
│   ├── app/
│   ├── config/
│   ├── drivers/
│   ├── platform/
│   ├── protocol/
│   ├── third_party/
│   ├── main.c
│   └── Makefile
├── ros2_bridge/
│   ├── bluepill_uart_bridge/
│   ├── launch/
│   ├── package.xml
│   ├── setup.cfg
│   ├── setup.py
│   └── test/
└── scripts/
    ├── debug_uart_rx_check.sh
    └── uart_link_check.py
```

## Hardware necesario

- Blue Pill STM32F103C8T6.
- ST-Link V2 o compatible.
- Adaptador USB-UART TTL de 3.3 V.
- Cables Dupont.
- PC con Ubuntu 24.04.


## Cableado

### ST-Link

```text
ST-Link SWDIO -> Blue Pill SWDIO
ST-Link SWCLK -> Blue Pill SWCLK
ST-Link GND   -> Blue Pill GND
ST-Link 3V3   -> Blue Pill 3V3
```

### USB-UART

Configuración por defecto:

- USART1
- TX Blue Pill: `PA9`
- RX Blue Pill: `PA10`
- `115200 8N1`

```text
Blue Pill PA9  -> RX del adaptador USB-UART
Blue Pill PA10 -> TX del adaptador USB-UART
Blue Pill GND  -> GND del adaptador USB-UART
```

Importante:

- usar niveles de **3.3 V**,
- no conectar RS232,
- no inyectar 5 V a los pines UART del STM32,
- asegurar masa común.

## Protocolo que deben implementar

Formato de trama:

```text
@LL:TTT:PAYLOAD:CC\n
```

Donde:

- `@` marca el inicio,
- `LL` es la longitud hexadecimal de `TTT:PAYLOAD`,
- `TTT` es el tipo (`CMD`, `DAT`, `STS`, `ACK`, `ERR`, `EVT`),
- `PAYLOAD` es texto ASCII,
- `CC` es checksum XOR de `LL:TTT:PAYLOAD`,
- `\n` marca fin de trama.

Ejemplos:

```text
@08:CMD:ping:52
@0B:CMD:status?:13
@0E:CMD:led=toggle:7D
```

Documentación teórica:

- [docs/PROTOCOL.md](docs/PROTOCOL.md)
- [docs/FRAME_GENERATION_AND_PARSING.md](docs/FRAME_GENERATION_AND_PARSING.md)
- [docs/PRACTICA_UART_ROS2.md](docs/PRACTICA_UART_ROS2.md)

## Instalación de ROS 2 y toolchain

Está detallada paso a paso en:

- [docs/SETUP_AND_SMOKETEST.md](docs/SETUP_AND_SMOKETEST.md)

Resumen rápido:

### Toolchain embebido

```bash
sudo apt update
sudo apt install -y \
  build-essential \
  make \
  git \
  gcc-arm-none-eabi \
  binutils-arm-none-eabi \
  gdb-multiarch \
  openocd \
  minicom \
  screen
```

### ROS 2 Jazzy en Ubuntu 24.04

```bash
sudo apt update
sudo apt install -y locales curl gnupg2 software-properties-common ca-certificates
sudo locale-gen en_US en_US.UTF-8
sudo update-locale LC_ALL=en_US.UTF-8 LANG=en_US.UTF-8
export LANG=en_US.UTF-8

sudo mkdir -p /etc/apt/keyrings
curl -sSL https://raw.githubusercontent.com/ros/rosdistro/master/ros.key | \
  sudo gpg --dearmor -o /etc/apt/keyrings/ros-archive-keyring.gpg
echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/ros-archive-keyring.gpg] http://packages.ros.org/ros2/ubuntu $(. /etc/os-release && echo $UBUNTU_CODENAME) main" | \
  sudo tee /etc/apt/sources.list.d/ros2.list > /dev/null
sudo apt update

sudo apt install -y \
  ros-jazzy-ros-base \
  python3-colcon-common-extensions \
  python3-rosdep \
  python3-serial

sudo rosdep init
rosdep update
echo 'source /opt/ros/jazzy/setup.bash' >> ~/.bashrc
source /opt/ros/jazzy/setup.bash
```

## Cómo compilar

### Firmware

```bash
cd firmware
make
```

```bash
make size
make flash
```

### Bridge ROS 2

```bash
cd ros2_bridge
source /opt/ros/jazzy/setup.bash
colcon build
source install/setup.bash
```

## Cómo lanzar el bridge

### Opción 1: `ros2 run`

```bash
cd ros2_bridge
source /opt/ros/jazzy/setup.bash
source install/setup.bash
ros2 run bluepill_uart_bridge serial_bridge_node --ros-args -p port:=/dev/ttyUSB0 -p baudrate:=115200
```

### Opción 2: `ros2 launch`

```bash
cd ros2_bridge
source /opt/ros/jazzy/setup.bash
source install/setup.bash
ros2 launch bluepill_uart_bridge bridge.launch.py port:=/dev/ttyUSB0 baudrate:=115200 poll_period_ms:=20
```

## Tópicos ROS 2 esperados

Publicados por el bridge:

- `/bridge/rx_raw`
- `/bridge/data`
- `/bridge/event`
- `/bridge/status`
- `/bridge/ack`
- `/bridge/error`
- `/bridge/tx_raw`

Suscriptos por el bridge:

- `/bridge/tx_cmd`
- `/bridge/tx_frame`

## Comandos útiles para probar

Ver lista de tópicos:

```bash
ros2 topic list
```

Ver telemetría:

```bash
ros2 topic echo /bridge/data
```

Ver ACKs:

```bash
ros2 topic echo /bridge/ack
```

Enviar `ping`:

```bash
ros2 topic pub --once /bridge/tx_cmd std_msgs/msg/String "{data: ping}"
```

Enviar `led=toggle`:

```bash
ros2 topic pub --once /bridge/tx_cmd std_msgs/msg/String "{data: led=toggle}"
```

Pedir estado:

```bash
ros2 topic pub --once /bridge/tx_cmd std_msgs/msg/String "{data: status?}"
ros2 topic echo /bridge/status
```

Enviar una trama completa desde ROS 2:

```bash
ros2 topic pub --once /bridge/tx_frame std_msgs/msg/String "{data: CMD:ping}"
```

## Scripts de diagnóstico

### Loopback del adaptador USB-UART

Unir TX y RX del adaptador entre sí, sin la placa:

```bash
python3 scripts/uart_link_check.py loopback /dev/ttyUSB0
```

### Prueba básica con Blue Pill

```bash
python3 scripts/uart_link_check.py bluepill /dev/ttyUSB0
```

### Debug con ST-Link + GDB/OpenOCD

```bash
scripts/debug_uart_rx_check.sh /dev/ttyUSB0
```

## Qué deberían tocar además del protocolo

- `firmware/app/sensors.c`
- `firmware/app/actuators.c`
- `firmware/app/app.c`

Si quieren agregar tópicos o adaptar la interfaz ROS 2:

- `ros2_bridge/bluepill_uart_bridge/serial_bridge_node.py`

## Criterio de éxito

La práctica está bien si logran al menos esto:

1. compilar y flashear la Blue Pill,
2. correr el bridge ROS 2,
3. enviar `ping` y recibir un `ACK`,
4. pedir `status?` y ver el tópico `/bridge/status`,
5. ver telemetría periódica en `/bridge/data`,
6. accionar al menos un actuador o LED real.

## Si algo no funciona

Ir primero a:

- [docs/SETUP_AND_SMOKETEST.md](docs/SETUP_AND_SMOKETEST.md)
- [docs/UART_TROUBLESHOOTING.md](docs/UART_TROUBLESHOOTING.md)

Si el problema parece del protocolo, revisen otra vez:

- framing,
- longitud `LL`,
- checksum XOR,
- delimitadores,
- reinicio del parser ante errores.
