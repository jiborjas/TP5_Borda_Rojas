#ifndef SENSORS_H
#define SENSORS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Inicializa sensores reales o simulados. */
void sensors_init(void);

/* Construye el payload DAT de telemetria. */
bool sensors_build_telemetry_payload(char *buffer, size_t buffer_size, uint32_t sequence);

#endif
