# Setup y smoke test

Esta guía está pensada para que un alumno pueda instalar todo desde cero y llegar a una validación mínima del proyecto.

## 1. Recomendación de entorno

Lo más simple para laboratorio es:

- **Ubuntu 24.04 nativo**, o
- una VM/Linux con passthrough USB.

WSL2 puede funcionar, pero suele agregar problemas con ST-Link y `/dev/ttyUSB*`.

## 2. Instalar dependencias embebidas

```bash
sudo apt update
sudo apt install -y \
  build-essential \
  make \
  git \
  gcc-arm-none-eabi \
  gdb-multiarch \
  binutils-arm-none-eabi \
  openocd \
  minicom \
  screen
```

Verificar:

```bash
arm-none-eabi-gcc --version
openocd --version
gdb-multiarch --version
```

## 3. Instalar ROS 2 Jazzy en Ubuntu 24.04

### 3.1 Locale y repositorio

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
```

### 3.2 Instalar paquetes ROS 2

```bash
sudo apt install -y \
  ros-jazzy-ros-base \
  python3-colcon-common-extensions \
  python3-rosdep \
  python3-serial
```

```bash
sudo rosdep init
rosdep update
echo 'source /opt/ros/jazzy/setup.bash' >> ~/.bashrc
source /opt/ros/jazzy/setup.bash
```

Verificar:

```bash
ros2 --help
colcon --help
```

## 4. Clonar el repo

```bash
git clone https://github.com/<organizacion-o-usuario>/bluepill-freertos-ros2-alumnos.git
cd bluepill-freertos-ros2-alumnos
```

## 5. Conectar hardware

### ST-Link

- `SWDIO`
- `SWCLK`
- `GND`
- `3V3`

### UART

- `PA9` Blue Pill -> `RX` del adaptador
- `PA10` Blue Pill -> `TX` del adaptador
- `GND` Blue Pill -> `GND` del adaptador

## 6. Permisos del puerto serie

```bash
ls -l /dev/ttyUSB0
id
```

Si no pertenecés a `dialout`:

```bash
sudo usermod -aG dialout "$USER"
```

Cerrar sesión y volver a entrar.

## 7. Compilar el firmware

```bash
cd firmware
make
make size
```

## 8. Flashear la Blue Pill

```bash
make flash
```

Si querés debug:

Terminal 1:

```bash
make openocd
```

Terminal 2:

```bash
make gdb
```

## 9. Compilar el bridge ROS 2

```bash
cd ../ros2_bridge
source /opt/ros/jazzy/setup.bash
colcon build
source install/setup.bash
```

## 10. Smoke test por etapas

### Etapa A: loopback del USB-UART

Sin Blue Pill, unir TX y RX del adaptador:

```bash
cd ..
python3 scripts/uart_link_check.py loopback /dev/ttyUSB0
```

Esperado:

```text
OK: loopback USB-UART funciona.
```

### Etapa B: bridge ROS 2 levanta

```bash
cd ros2_bridge
source /opt/ros/jazzy/setup.bash
source install/setup.bash
ros2 run bluepill_uart_bridge serial_bridge_node --ros-args -p port:=/dev/ttyUSB0 -p baudrate:=115200
```

Si todavía no implementaron el protocolo, el nodo puede levantar pero no comunicarse correctamente. Eso es esperable.

### Etapa C: publicar comandos desde ROS 2

Otra terminal:

```bash
source /opt/ros/jazzy/setup.bash
cd ros2_bridge
source install/setup.bash
ros2 topic pub --once /bridge/tx_cmd std_msgs/msg/String "{data: ping}"
```

### Etapa D: observar respuestas

```bash
ros2 topic echo /bridge/ack
ros2 topic echo /bridge/status
ros2 topic echo /bridge/data
ros2 topic echo /bridge/error
```

## 11. Qué debería verse cuando esté terminado

Una vez implementado el protocolo, deberían poder lograr:

- `ping` -> respuesta `ACK`,
- `status?` -> mensaje `STS`,
- telemetría periódica en `/bridge/data`,
- tramas visibles en `/bridge/rx_raw` y `/bridge/tx_raw`.

## 12. Si no funciona

Revisar en este orden:

1. cableado,
2. puerto correcto (`/dev/ttyUSB0`, `/dev/ttyUSB1`, etc.),
3. permisos `dialout`,
4. si `make flash` realmente programó la placa,
5. si el bridge ROS 2 abrió el puerto,
6. framing `@LL:TTT:PAYLOAD:CC\n`,
7. checksum XOR,
8. reinicio del parser ante errores.

Listar puertos:

```bash
ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null
```

Opcionalmente mirar mensajes del kernel:

```bash
dmesg | tail -n 30
```

Suponiendo que el adaptador aparezca como `/dev/ttyUSB0`, ese será el puerto a usar.

## 10. Compilar el bridge ROS 2

Ir al paquete ROS 2:

```bash
cd /home/lamet/proyectos-embebidos/bluepill-freertos-ros2/ros2_bridge
source /opt/ros/jazzy/setup.bash
colcon build
source install/setup.bash
```

No hace falta generar nodos nuevos: el nodo ya existe en el repo y se llama `serial_bridge_node`.

## 11. Correr el bridge ROS 2

### Opción A: launch file

Si el puerto es `/dev/ttyUSB0`:

```bash
cd /home/lamet/proyectos-embebidos/bluepill-freertos-ros2/ros2_bridge
source /opt/ros/jazzy/setup.bash
source install/setup.bash
ros2 launch bluepill_uart_bridge bridge.launch.py
```

### Opción B: correr el nodo manualmente

Esto es mejor si el puerto no es `/dev/ttyUSB0`:

```bash
cd /home/lamet/proyectos-embebidos/bluepill-freertos-ros2/ros2_bridge
source /opt/ros/jazzy/setup.bash
source install/setup.bash
ros2 run bluepill_uart_bridge serial_bridge_node --ros-args -p port:=/dev/ttyUSB0 -p baudrate:=115200
```

## 12. Ver los tópicos disponibles

En otra terminal:

```bash
source /opt/ros/jazzy/setup.bash
cd /home/lamet/proyectos-embebidos/bluepill-freertos-ros2/ros2_bridge
source install/setup.bash
ros2 topic list
```

Deberías ver, entre otros:

- `/bridge/rx_raw`
- `/bridge/data`
- `/bridge/event`
- `/bridge/status`
- `/bridge/ack`
- `/bridge/error`
- `/bridge/tx_cmd`
- `/bridge/tx_frame`

## 13. Prueba mínima extremo a extremo

### Terminal 1

Dejar corriendo el bridge ROS 2.

### Terminal 2: escuchar ACK

```bash
source /opt/ros/jazzy/setup.bash
cd /home/lamet/proyectos-embebidos/bluepill-freertos-ros2/ros2_bridge
source install/setup.bash
ros2 topic echo /bridge/ack
```

### Terminal 3: escuchar estado

```bash
source /opt/ros/jazzy/setup.bash
cd /home/lamet/proyectos-embebidos/bluepill-freertos-ros2/ros2_bridge
source install/setup.bash
ros2 topic echo /bridge/status
```

### Terminal 4: mandar `ping`

```bash
source /opt/ros/jazzy/setup.bash
cd /home/lamet/proyectos-embebidos/bluepill-freertos-ros2/ros2_bridge
source install/setup.bash
ros2 topic pub --once /bridge/tx_cmd std_msgs/msg/String '{data: "ping"}'
```

Resultado esperado:

- en `/bridge/ack` deberías ver algo equivalente a `pong=1`.

### Terminal 4: mandar toggle del LED

```bash
ros2 topic pub --once /bridge/tx_cmd std_msgs/msg/String '{data: "led=toggle"}'
```

Resultado esperado:

- el LED onboard de la Blue Pill cambia de estado,
- puede aparecer un `ACK` en `/bridge/ack`.

### Terminal 4: pedir estado

```bash
ros2 topic pub --once /bridge/tx_cmd std_msgs/msg/String '{data: "status?"}'
```

Resultado esperado:

- en `/bridge/status` deberías recibir una respuesta con contadores o estado actual.

## 14. Validación

La validación mínima queda aprobada si se cumple esto:

1. el firmware compila,
2. la placa se flashea,
3. el bridge ROS 2 abre el puerto serie,
4. `ping` devuelve `ACK`,
5. `led=toggle` modifica el LED,
6. `status?` o la telemetría llegan a ROS 2.

Si eso funciona, ya está validado el camino:

Blue Pill <-> UART <-> ROS 2

Ese es el prerrequisito para después integrar con los robots.

## 15. Troubleshooting rápido

### `make flash` falla

Revisar:

- cableado SWD,
- alimentación de la placa,
- acceso al ST-Link,
- si estás en WSL2, si el ST-Link está realmente adjuntado.

### El bridge no abre el puerto serie

Revisar:

- nombre real del dispositivo (`/dev/ttyUSB0`, `/dev/ttyACM0`, etc.),
- permisos sobre el puerto,
- si el USB-UART está adjuntado a WSL,
- si el baudrate es `115200`.

### No llegan ACK ni status

Revisar:

- cruce TX/RX,
- masa común entre Blue Pill y adaptador,
- que el firmware correcto esté flasheado,
- que el bridge esté usando el puerto correcto,
- que la Blue Pill esté realmente corriendo tras el reset.

Si la Blue Pill transmite `DAT`/`STS` pero no responde comandos, seguir la guía detallada:

```bash
python3 scripts/uart_link_check.py loopback /dev/ttyUSB0
python3 scripts/uart_link_check.py bluepill /dev/ttyUSB0
scripts/debug_uart_rx_check.sh /dev/ttyUSB0
```

Ver también:

- `docs/UART_TROUBLESHOOTING.md`

En la integración original se encontró un HardFault causado por usar `sscanf()` dentro del parser hexadecimal. La solución fue reemplazarlo por conversión manual liviana en `firmware/protocol/parser.c`.

### El LED no cambia

Revisar:

- que sea una Blue Pill con LED en `PC13`,
- que el comando `led=toggle` haya sido recibido,
- que haya un `ACK` o algún mensaje en `/bridge/error`.

## 16. Próximo paso recomendado

Una vez validado este smoke test, el siguiente objetivo ya no es instalar nada sino empezar a adaptar la base para la integración con el sistema real y, más adelante, con los robots de la universidad.
