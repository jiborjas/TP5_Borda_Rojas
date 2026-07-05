#include <stdbool.h>
#include <stdio.h>

#include "sensors.h"

void sensors_init(void)
{
    //Los sensores están simulados, no hay hardware real. 
    // No se necesita inicialización.
}


// Simula la lectura de un sensor de temperatura.
bool sensors_build_telemetry_payload(char *buffer, size_t buffer_size, uint32_t sequence)
{
    int written;
    unsigned int pseudo_value;

    if ((buffer == NULL) || (buffer_size == 0U)) {
        return false;
    }   // verifica que el buffer sea válido.

    pseudo_value = 20U + (sequence % 10U);  //Valor entre 20 y 29.

    // Imprime un valor pseudoaleatorio y un contador de secuencia. 
    // En un sistema real, se leerían sensores de temperatura, humedad, presión, etc.
    written = snprintf(buffer, buffer_size, "temp=%u,seq=%lu",
                       pseudo_value,
                       (unsigned long) sequence);

	// Verifica que el snprintf haya escrito una cadena de tamaño coherente.
    return (written > 0) && ((size_t) written < buffer_size);
}   