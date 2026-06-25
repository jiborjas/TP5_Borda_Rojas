#ifndef TASKS_H
#define TASKS_H

#include <stdint.h>

/* Crea colas, crea tareas y arranca el scheduler FreeRTOS. */
void tasks_start(void);

/* Devuelve pb: bytes consumidos por el parser. */
uint32_t tasks_get_parser_byte_count(void);

/* Devuelve pm: mensajes validos entregados por el parser. */
uint32_t tasks_get_parser_message_count(void);

/* Devuelve pe: errores detectados por el parser. */
uint32_t tasks_get_parser_error_count(void);

#endif
