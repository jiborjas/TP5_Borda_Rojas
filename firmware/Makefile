# Proyecto base STM32F103C8T6 con libopencm3 + FreeRTOS
PROJECT = tp5_borda_rojas
TARGET = $(PROJECT).elf
BUILD_DIR = bin
APP_CONFIG_HEADER = config/app_config.h

# Rutas base
THIRD_PARTY_DIR ?= third_party
COMMON_DIR ?= $(THIRD_PARTY_DIR)/common
LIBOPENCM3_DIR ?= $(THIRD_PARTY_DIR)/libopencm3
FREERTOS_DIR ?= $(THIRD_PARTY_DIR)/FreeRTOS-Kernel

HEAP_4_SRC := $(FREERTOS_DIR)/portable/MemMang/heap_4.c
ifeq ($(wildcard $(HEAP_4_SRC)),)
HEAP_4_SRC := $(FREERTOS_DIR)/heap_4.c
endif

MCU = cortex-m3
CDEFS = -DSTM32F1

# Compilador
CC = arm-none-eabi-gcc
OBJCOPY = arm-none-eabi-objcopy
OBJDUMP = arm-none-eabi-objdump
SIZE = arm-none-eabi-size
OPENOCD ?= openocd
GDB ?= gdb-multiarch
OPENOCD_INTERFACE ?= interface/stlink.cfg
OPENOCD_TARGET ?= target/stm32f1x.cfg

INCLUDES  = -I$(LIBOPENCM3_DIR)/include
INCLUDES += -I$(COMMON_DIR)
INCLUDES += -I$(FREERTOS_DIR)
INCLUDES += -I$(FREERTOS_DIR)/include
INCLUDES += -I$(FREERTOS_DIR)/portable/GCC/ARM_CM3
INCLUDES += -I./config
INCLUDES += -I./app
INCLUDES += -I./drivers
INCLUDES += -I./platform
INCLUDES += -I./protocol
INCLUDES += -I./

# Flags e linker
CFLAGS = -mcpu=$(MCU) -mthumb -Wall -Wextra -O0 -g -ffunction-sections -fdata-sections $(INCLUDES) $(CDEFS) -nostdlib
LDSCRIPT = $(COMMON_DIR)/linker.ld
LDFLAGS = -nostartfiles --specs=nano.specs -Wl,--gc-sections -Wl,-Map=$(BUILD_DIR)/$(PROJECT).map -T$(LDSCRIPT)
LDLIBS = -L$(LIBOPENCM3_DIR)/lib -lopencm3_stm32f1 -lc -lgcc -lnosys

FREERTOS_SRC = \
    $(FREERTOS_DIR)/list.c \
    $(FREERTOS_DIR)/queue.c \
    $(FREERTOS_DIR)/tasks.c \
    $(FREERTOS_DIR)/portable/GCC/ARM_CM3/port.c \
    $(HEAP_4_SRC)

LOCAL_SRC = \
    main.c \
    platform/system_init.c \
    app/tasks.c \
    drivers/uart_comm.c \
    protocol/protocol.c \
    protocol/parser.c \
    app/app.c \
    app/sensors.c \
    app/actuators.c

LOCAL_OBJ = $(patsubst %.c,$(BUILD_DIR)/%.o,$(LOCAL_SRC))
FREERTOS_OBJ = $(patsubst $(FREERTOS_DIR)/%.c,$(BUILD_DIR)/freertos/%.o,$(FREERTOS_SRC))
OBJ = $(LOCAL_OBJ) $(FREERTOS_OBJ)

.PHONY: all clean rebuild flash erase size list disasm artifacts help check-deps test-host
.PHONY: openocd gdb bin hex

all: check-deps $(LIBOPENCM3_DIR)/lib/libopencm3_stm32f1.a $(BUILD_DIR)/$(TARGET)

check-deps:
	@test -f "$(COMMON_DIR)/linker.ld" || (echo "Falta $(COMMON_DIR)/linker.ld" && exit 1)
	@test -f "$(LIBOPENCM3_DIR)/include/libopencm3/stm32/usart.h" || (echo "Falta libopencm3 en $(LIBOPENCM3_DIR)" && exit 1)
	@test -f "$(FREERTOS_DIR)/include/FreeRTOS.h" || (echo "Falta FreeRTOS-Kernel en $(FREERTOS_DIR)" && exit 1)

$(OBJ): $(APP_CONFIG_HEADER)

$(BUILD_DIR)/$(TARGET): $(OBJ) $(LIBOPENCM3_DIR)/lib/libopencm3_stm32f1.a
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(LDLIBS)
	$(OBJCOPY) -O ihex $@ $(BUILD_DIR)/$(PROJECT).hex
	$(OBJCOPY) -O binary $@ $(BUILD_DIR)/$(PROJECT).bin

hex: $(BUILD_DIR)/$(PROJECT).hex

bin: $(BUILD_DIR)/$(PROJECT).bin

$(BUILD_DIR)/$(PROJECT).hex: $(BUILD_DIR)/$(TARGET)
	$(OBJCOPY) -O ihex $< $@

$(BUILD_DIR)/$(PROJECT).bin: $(BUILD_DIR)/$(TARGET)
	$(OBJCOPY) -O binary $< $@

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/freertos/%.o: $(FREERTOS_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(LIBOPENCM3_DIR)/lib/libopencm3_stm32f1.a:
	$(MAKE) -C $(LIBOPENCM3_DIR)

flash: $(BUILD_DIR)/$(TARGET)
	$(OPENOCD) -f $(OPENOCD_INTERFACE) -f $(OPENOCD_TARGET) -c "program $< verify reset exit"

erase:
	$(OPENOCD) -f $(OPENOCD_INTERFACE) -f $(OPENOCD_TARGET) -c "init; reset halt; stm32f1x mass_erase 0; reset; exit"

openocd:
	$(OPENOCD) -f $(OPENOCD_INTERFACE) -f $(OPENOCD_TARGET)

gdb: $(BUILD_DIR)/$(TARGET)
	$(GDB) $< -ex "target remote localhost:3333"

size: $(BUILD_DIR)/$(TARGET)
	$(SIZE) $<

list: $(BUILD_DIR)/$(TARGET)
	$(OBJDUMP) -S $< > $(BUILD_DIR)/$(PROJECT).lst

disasm: list

artifacts: all size list

test-host:
	$(MAKE) -C ../tests/host run

clean:
	rm -rf $(BUILD_DIR)

rebuild: clean all

help:
	@echo "Comandos disponibles:"
	@echo "  make            -> Compila y genera .elf, .hex y .bin"
	@echo "  make hex        -> Genera/actualiza el archivo Intel HEX"
	@echo "  make bin        -> Genera/actualiza el binario crudo"
	@echo "  make flash      -> Programa la Blue Pill con OpenOCD/ST-Link"
	@echo "  make erase      -> Borra la flash completa del STM32F103"
	@echo "  make openocd    -> Inicia servidor OpenOCD"
	@echo "  make gdb        -> Abre GDB y conecta a localhost:3333"
	@echo "  make size       -> Muestra uso de Flash/RAM"
	@echo "  make list       -> Genera listado con desensamblado en bin/"
	@echo "  make artifacts  -> Compila, muestra size y genera .lst"
	@echo "  make test-host  -> Corre tests de protocolo/parser en la PC"
	@echo "  make clean      -> Borra archivos generados"
	@echo "  make rebuild    -> Limpia y recompila"
	@echo "  make help       -> Muestra esta ayuda"
