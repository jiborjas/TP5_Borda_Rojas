#ifndef APP_H
#define APP_H

#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "queue.h"

#include "../protocol/protocol.h"

/* Orden simple que app.c manda a actuators.c. */
typedef struct {
    /* Nombre del actuador; hoy usamos "led". */
    char target[16];

    /* Accion sobre el actuador; hoy usamos "on", "off" o "toggle". */
    char action[16];
} actuator_command_t;

/* Reinicia contadores de aplicacion. */
void app_init(void);

/* Procesa un mensaje completo ya validado por el parser. */
void app_handle_message(const protocol_message_t *message, QueueHandle_t tx_queue, QueueHandle_t actuator_queue);

/* Arma un mensaje DAT periodico con telemetria simulada. */
bool app_build_telemetry_message(protocol_message_t *message, uint32_t sequence);

/* Arma un mensaje STS con contadores internos. */
void app_build_status_message(protocol_message_t *message);

#endif
