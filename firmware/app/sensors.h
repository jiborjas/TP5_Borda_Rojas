#ifndef SENSORS_H
#define SENSORS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void sensors_init(void);
bool sensors_build_telemetry_payload(char *buffer, size_t buffer_size, uint32_t sequence);

#endif
