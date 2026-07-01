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

    /*
     * Las respuestas no se mandan directo por UART desde la aplicacion. Se
     * ponen en una cola y la tarea task_uart_tx las convierte a trama
     * @LL:TTT:PAYLOAD:CC\n. Esta separacion evita mezclar logica de protocolo,
     * aplicacion y hardware.
     */
    if (protocol_message_set(&message, type, payload)) {
        xQueueSend(tx_queue, &message, portMAX_DELAY);
    }
}

static bool payload_equals(const char *payload, const char *expected)
{
    /* Pequeño helper para que las comparaciones de comandos sean legibles. */
    return strcmp(payload, expected) == 0;
}

static bool build_led_command(const char *payload, actuator_command_t *command)
{
    /*
     * La aplicacion entiende comandos de texto, pero el modulo de actuadores
     * recibe una estructura con target/action. Asi el parseo del comando queda
     * aca y el control fisico del pin PC13 queda en actuators.c.
     */
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

    /*
     * Por consigna, la Blue Pill solo acepta comandos entrantes de tipo CMD.
     * Si llega ACK, STS, ERR u otro tipo, no se ejecuta nada y se responde con
     * un error de aplicacion.
     */
    if (message->type != PROTOCOL_TYPE_CMD) {
        send_simple_message(tx_queue, PROTOCOL_TYPE_ERR, "code=unexpected_type");
        g_error_count++;
        return;
    }

    /*
     * Comandos de LED. PC13 en la Blue Pill es activo en bajo; esa inversion se
     * maneja en actuators.c para que aca solo pensemos en "on/off/toggle".
     */
    if (build_led_command(message->payload, &command)) {
        xQueueSend(actuator_queue, &command, portMAX_DELAY);
        send_simple_message(tx_queue, PROTOCOL_TYPE_ACK, "cmd=ok");
        return;
    }

    /* ping no toca hardware; sirve para confirmar que el enlace responde. */
    if (payload_equals(message->payload, "ping")) {
        send_simple_message(tx_queue, PROTOCOL_TYPE_ACK, "pong=1");
        return;
    }

    /*
     * status? devuelve contadores internos. Esto ayuda a diagnosticar si fallan
     * bytes UART, parser, cola o aplicacion sin tener que mirar registros del
     * microcontrolador en vivo.
     */
    if (payload_equals(message->payload, "status?")) {
        protocol_message_t status_message;
        app_build_status_message(&status_message);
        xQueueSend(tx_queue, &status_message, portMAX_DELAY);
        return;
    }

    /* Cualquier payload CMD desconocido se informa explicitamente. */
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

    /*
     * Formato pedido por la consigna:
     * rx = mensajes recibidos por la aplicacion
     * ae = errores de aplicacion
     * irq = bytes recibidos por interrupcion UART
     * pb = bytes procesados por el parser
     * pm = mensajes validos entregados por el parser
     * pe = errores detectados por el parser
     * qd = bytes descartados por cola llena
     */
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
