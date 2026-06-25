#include <stdbool.h>
#include <stdio.h>

#include "sensors.h"

/* Inicializa sensores reales o simulados. */
void sensors_init(void)
{
    /* En este TP no hay sensor fisico: la telemetria se simula. */
}

/* Construye un payload DAT para probar telemetria periodica. */
bool sensors_build_telemetry_payload(char *buffer, size_t buffer_size, uint32_t sequence)
{
    /* snprintf devuelve cantidad de caracteres escritos. */
    int written;

    /* Valor simulado de temperatura para ver cambios en el monitor. */
    unsigned int pseudo_value;

    /* Sin buffer o con tamano cero no podemos escribir. */
    if ((buffer == NULL) || (buffer_size == 0U)) {
        return false;
    }

    /* Temperatura ficticia entre 20 y 29. */
    pseudo_value = 20U + (sequence % 10U);

    /* Payload estilo clave=valor, facil de leer desde ROS 2. */
    written = snprintf(buffer, buffer_size, "temp=%u,seq=%lu",
                       pseudo_value,
                       (unsigned long) sequence);

    /* Retorna true solo si el texto entro completo. */
    return (written > 0) && ((size_t) written < buffer_size);
}
