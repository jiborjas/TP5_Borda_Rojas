#ifndef APP_H
#define APP_H

#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "queue.h"

#include "../protocol/protocol.h"

typedef struct {
    char target[16];
    char action[16];
} actuator_command_t;

void app_init(void);
void app_handle_message(const protocol_message_t *message, QueueHandle_t tx_queue, QueueHandle_t actuator_queue);
bool app_build_telemetry_message(protocol_message_t *message, uint32_t sequence);
void app_build_status_message(protocol_message_t *message);

#endif
