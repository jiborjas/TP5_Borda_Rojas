# Makefile raiz del TP5.
#
# Permite ejecutar los comandos clasicos de la catedra desde TP5_Borda_Rojas/
# y delega la compilacion real al Makefile autocontenido de firmware/.

FIRMWARE_DIR := firmware

.PHONY: all clean rebuild flash erase size list disasm artifacts help
.PHONY: openocd gdb bin hex test-host

all:
	$(MAKE) -C $(FIRMWARE_DIR) all

clean:
	$(MAKE) -C $(FIRMWARE_DIR) clean

rebuild:
	$(MAKE) -C $(FIRMWARE_DIR) rebuild

flash:
	$(MAKE) -C $(FIRMWARE_DIR) flash

erase:
	$(MAKE) -C $(FIRMWARE_DIR) erase

size:
	$(MAKE) -C $(FIRMWARE_DIR) size

list:
	$(MAKE) -C $(FIRMWARE_DIR) list

disasm:
	$(MAKE) -C $(FIRMWARE_DIR) disasm

artifacts:
	$(MAKE) -C $(FIRMWARE_DIR) artifacts

openocd:
	$(MAKE) -C $(FIRMWARE_DIR) openocd

gdb:
	$(MAKE) -C $(FIRMWARE_DIR) gdb

bin:
	$(MAKE) -C $(FIRMWARE_DIR) bin

hex:
	$(MAKE) -C $(FIRMWARE_DIR) hex

test-host:
	$(MAKE) -C $(FIRMWARE_DIR) test-host

help:
	$(MAKE) -C $(FIRMWARE_DIR) help
