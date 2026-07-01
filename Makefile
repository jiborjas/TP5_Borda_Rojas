# Makefile raiz para trabajar desde TP5_Borda_Rojas.
#
# El firmware real vive en firmware/ y tiene su propio Makefile. Este archivo
# solo reenvia los comandos comunes para que `make flash` funcione desde la raiz
# del TP, que es donde normalmente queda parada la terminal.

.PHONY: all flash size clean openocd gdb firmware-test help

all:
	$(MAKE) -C firmware

flash:
	$(MAKE) -C firmware flash

size:
	$(MAKE) -C firmware size

clean:
	$(MAKE) -C firmware clean
	$(MAKE) -C tests/host clean

openocd:
	$(MAKE) -C firmware openocd

gdb:
	$(MAKE) -C firmware gdb

firmware-test:
	$(MAKE) -C tests/host test

help:
	@echo "Comandos desde la raiz del TP:"
	@echo "  make              -> compila firmware"
	@echo "  make flash        -> flashea via OpenOCD/ST-Link"
	@echo "  make size         -> muestra uso de memoria"
	@echo "  make firmware-test -> corre tests host"
	@echo "  make openocd      -> levanta OpenOCD"
	@echo "  make gdb          -> conecta GDB"
	@echo "  make clean        -> limpia firmware y tests host"
