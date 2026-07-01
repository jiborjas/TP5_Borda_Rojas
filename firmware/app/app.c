#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "app.h"
#include "sensors.h"
#include "tasks.h"
#include "../drivers/uart_comm.h"
#include "../protocol/protocol.h"

static uint32_t g_rx_count = 0U;
static uint32_t g_error_count = 0U;

static void send_simple_message(QueueHandle_t tx_queue, protocol_type_t type, const char *payload)
{
    protocol_message_t message;

    if (protocol_message_set(&message, type, payload)) {
        xQueueSend(tx_queue, &message, portMAX_DELAY);
    }
}

static bool payload_equals(const char *payload, const char *expected)
{
    return strcmp(payload, expected) == 0;
}

static bool build_led_command(const char *payload, actuator_command_t *command)
{
    if (payload_equals(payload, "led=on")) {
        strncpy(command->target, "led", sizeof(command->target) - 1U);
        strncpy(command->action, "on", sizeof(command->action) - 1U);
        return true;
    }

    if (payload_equals(payload, "led=off")) {
        strncpy(command->target, "led", sizeof(command->target) - 1U);
        strncpy(command->action, "off", sizeof(command->action) - 1U);
        return true;
    }

    if (payload_equals(payload, "led=toggle")) {
        strncpy(command->target, "led", sizeof(command->target) - 1U);
        strncpy(command->action, "toggle", sizeof(command->action) - 1U);
        return true;
    }

    return false;
}

void app_init(void)
{
    g_rx_count = 0U;
    g_error_count = 0U;
}

void app_handle_message(const protocol_message_t *message, QueueHandle_t tx_queue, QueueHandle_t actuator_queue)
{
    actuator_command_t command = {0};

    if ((message == NULL) || (tx_queue == NULL) || (actuator_queue == NULL)) {
        return;
    }

    g_rx_count++;

    if (message->type != PROTOCOL_TYPE_CMD) {
        send_simple_message(tx_queue, PROTOCOL_TYPE_ERR, "code=unexpected_type");
        g_error_count++;
        return;
    }

    if (build_led_command(message->payload, &command)) {
        xQueueSend(actuator_queue, &command, portMAX_DELAY);
        send_simple_message(tx_queue, PROTOCOL_TYPE_ACK, "cmd=ok");
        return;
    }

    if (payload_equals(message->payload, "ping")) {
        send_simple_message(tx_queue, PROTOCOL_TYPE_ACK, "pong=1");
        return;
    }

    if (payload_equals(message->payload, "status?")) {
        protocol_message_t status_message;
        app_build_status_message(&status_message);
        xQueueSend(tx_queue, &status_message, portMAX_DELAY);
        return;
    }

    send_simple_message(tx_queue, PROTOCOL_TYPE_ERR, "code=unknown_cmd");
    g_error_count++;
}

bool app_build_telemetry_message(protocol_message_t *message, uint32_t sequence)
{
    char payload[PROTOCOL_MAX_PAYLOAD_LENGTH + 1U];

    if (message == NULL) {
        return false;
    }

    if (!sensors_build_telemetry_payload(payload, sizeof(payload), sequence)) {
        return false;
    }

    return protocol_message_set(message, PROTOCOL_TYPE_DAT, payload);
}

void app_build_status_message(protocol_message_t *message)
{
    char payload[PROTOCOL_MAX_PAYLOAD_LENGTH + 1U];

    if (message == NULL) {
        return;
    }

    snprintf(payload, sizeof(payload), "rx=%lu,ae=%lu,irq=%lu,pb=%lu,pm=%lu,pe=%lu,qd=%lu",
              (unsigned long) g_rx_count,
              (unsigned long) g_error_count,
              (unsigned long) uart_comm_get_rx_irq_count(),
              (unsigned long) tasks_get_parser_byte_count(),
              (unsigned long) tasks_get_parser_message_count(),
              (unsigned long) tasks_get_parser_error_count(),
              (unsigned long) uart_comm_get_rx_drop_count());
    (void) protocol_message_set(message, PROTOCOL_TYPE_STS, payload);
}
