# Tests de PC para el TP5.
#
# No compilan FreeRTOS ni libopencm3. Solo prueban el codigo de protocolo
# que es C puro y puede ejecutarse en la computadora.

CC ?= gcc
CFLAGS := -std=c11 -Wall -Wextra -Werror -I../../firmware/protocol -I../../firmware/config

PROTOCOL_SRC := ../../firmware/protocol/protocol.c
PARSER_SRC := ../../firmware/protocol/parser.c

.PHONY: all run clean

all: test_protocol test_parser

test_protocol: test_protocol.c $(PROTOCOL_SRC) ../../firmware/protocol/protocol.h ../../firmware/config/app_config.h
	$(CC) $(CFLAGS) test_protocol.c $(PROTOCOL_SRC) -o $@

test_parser: test_parser.c $(PROTOCOL_SRC) $(PARSER_SRC) ../../firmware/protocol/protocol.h ../../firmware/protocol/parser.h ../../firmware/config/app_config.h
	$(CC) $(CFLAGS) test_parser.c $(PROTOCOL_SRC) $(PARSER_SRC) -o $@

run: all
	./test_protocol
	./test_parser

clean:
	rm -f test_protocol test_parser
