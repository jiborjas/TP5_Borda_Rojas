#include <stdbool.h>
#include <stdio.h>

#include "sensors.h"

void sensors_init(void)
{
}

bool sensors_build_telemetry_payload(char *buffer, size_t buffer_size, uint32_t sequence)
{
    int written;
    unsigned int pseudo_value;

    if ((buffer == NULL) || (buffer_size == 0U)) {
        return false;
    }

    pseudo_value = 20U + (sequence % 10U);
    written = snprintf(buffer, buffer_size, "temp=%u,seq=%lu",
                       pseudo_value,
                       (unsigned long) sequence);
    return (written > 0) && ((size_t) written < buffer_size);
}
