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

/*
 * Logica de aplicacion del TP5.
 *
 * Esta capa ya no interpreta bytes de UART. Recibe mensajes validados por el
 * parser y decide la respuesta: ACK para comandos aceptados, STS para estado y
 * ERR para errores de aplicacion. El LED PC13 se maneja mediante una cola hacia
 * actuadores para no mezclar despacho de protocolo con acceso directo a GPIO.
 */

/* rx del status: mensajes validos que llegaron hasta la aplicacion. */
static uint32_t g_rx_count = 0U;

/* ae del status: errores decididos por la aplicacion. */
static uint32_t g_error_count = 0U;

/* Encola una respuesta simple para que task_uart_tx la mande por USART1. */
static void send_simple_message(QueueHandle_t tx_queue, protocol_type_t type, const char *payload)
{
    /* Mensaje logico que despues se codifica como @LL:TTT:PAYLOAD:CC\n. */
    protocol_message_t message;

    /* protocol_message_set valida tipo, longitud y caracteres del payload. */
    if (protocol_message_set(&message, type, payload)) {
        /* La cola desacopla app.c de la transmision UART bloqueante. */
        xQueueSend(tx_queue, &message, portMAX_DELAY);
    }
}

/* Wrapper chico para que las comparaciones de comandos se lean mas claro. */
static bool payload_equals(const char *payload, const char *expected)
{
    /* strcmp devuelve 0 cuando los strings son iguales. */
    return strcmp(payload, expected) == 0;
}

/* Convierte payloads led=... en una orden para el modulo de actuadores. */
static bool build_led_command(const char *payload, actuator_command_t *command)
{
    /* led=on enciende el LED integrado de PC13. */
    if (payload_equals(payload, "led=on")) {
        strncpy(command->target, "led", sizeof(command->target) - 1U);
        strncpy(command->action, "on", sizeof(command->action) - 1U);
        return true;
    }

    /* led=off apaga el LED integrado de PC13. */
    if (payload_equals(payload, "led=off")) {
        strncpy(command->target, "led", sizeof(command->target) - 1U);
        strncpy(command->action, "off", sizeof(command->action) - 1U);
        return true;
    }

    /* led=toggle invierte el estado actual del LED. */
    if (payload_equals(payload, "led=toggle")) {
        strncpy(command->target, "led", sizeof(command->target) - 1U);
        strncpy(command->action, "toggle", sizeof(command->action) - 1U);
        return true;
    }

    /* Si no coincide con ningun comando LED, la app seguira probando otros comandos. */
    return false;
}

/* Inicializa contadores propios de la aplicacion. */
void app_init(void)
{
    /* Al arrancar firmware, todavia no procesamos mensajes. */
    g_rx_count = 0U;

    /* Al arrancar firmware, todavia no detectamos errores de aplicacion. */
    g_error_count = 0U;
}

/* Atiende un mensaje completo y validado por el parser. */
void app_handle_message(const protocol_message_t *message, QueueHandle_t tx_queue, QueueHandle_t actuator_queue)
{
    /* Orden que se manda a task_actuators si el comando era de LED. */
    actuator_command_t command = {0};

    /* Si falta algun puntero critico, salimos sin tocar colas ni hardware. */
    if ((message == NULL) || (tx_queue == NULL) || (actuator_queue == NULL)) {
        return;
    }

    /* Contamos todo mensaje valido que llego a la capa de aplicacion. */
    g_rx_count++;

    /* Este firmware espera recibir comandos desde la PC: tipo CMD. */
    if (message->type != PROTOCOL_TYPE_CMD) {
        /* Un tipo distinto no se ejecuta como comando. */
        send_simple_message(tx_queue, PROTOCOL_TYPE_ERR, "code=unexpected_type");

        /* Registramos error de aplicacion para status?. */
        g_error_count++;

        /* Ya respondimos, no seguimos procesando. */
        return;
    }

    /* Si el payload era led=on/off/toggle, armamos una orden para PC13. */
    if (build_led_command(message->payload, &command)) {
        /* El GPIO se toca en task_actuators, no aca. */
        xQueueSend(actuator_queue, &command, portMAX_DELAY);

        /* Confirmamos que el comando fue aceptado. */
        send_simple_message(tx_queue, PROTOCOL_TYPE_ACK, "cmd=ok");

        /* Comando resuelto. */
        return;
    }

    /* ping sirve para validar enlace, parser y respuesta. */
    if (payload_equals(message->payload, "ping")) {
        /* La respuesta esperada por el PDF es ACK:pong=1. */
        send_simple_message(tx_queue, PROTOCOL_TYPE_ACK, "pong=1");

        /* Comando resuelto. */
        return;
    }

    /* status? devuelve contadores internos de la etapa 3. */
    if (payload_equals(message->payload, "status?")) {
        /* Mensaje STS que se va a llenar abajo. */
        protocol_message_t status_message;

        /* Armamos el payload rx=...,ae=...,irq=...,pb=...,pm=...,pe=...,qd=.... */
        app_build_status_message(&status_message);

        /* Encolamos STS para que salga por UART. */
        xQueueSend(tx_queue, &status_message, portMAX_DELAY);

        /* Comando resuelto. */
        return;
    }

    /* Si llegamos aca, era CMD pero el payload no era conocido. */
    send_simple_message(tx_queue, PROTOCOL_TYPE_ERR, "code=unknown_cmd");

    /* Registramos error de aplicacion para status?. */
    g_error_count++;
}

/* Construye telemetria periodica DAT, util para el bridge ROS 2. */
bool app_build_telemetry_message(protocol_message_t *message, uint32_t sequence)
{
    /* Buffer local para "temp=N,seq=N". */
    char payload[PROTOCOL_MAX_PAYLOAD_LENGTH + 1U];

    /* Sin mensaje destino no podemos devolver nada. */
    if (message == NULL) {
        return false;
    }

    /* sensors.c arma un valor simulado para poder probar sin sensor real. */
    if (!sensors_build_telemetry_payload(payload, sizeof(payload), sequence)) {
        return false;
    }

    /* DAT indica dato/telemetria. */
    return protocol_message_set(message, PROTOCOL_TYPE_DAT, payload);
}

/* Construye el mensaje STS pedido por la etapa 3. */
void app_build_status_message(protocol_message_t *message)
{
    /* Payload completo del estado. */
    char payload[PROTOCOL_MAX_PAYLOAD_LENGTH + 1U];

    /* Cantidad de caracteres que intento escribir snprintf(). */
    int written;

    /* Sin mensaje destino no hay nada que llenar. */
    if (message == NULL) {
        return;
    }

    /*
     * Significado de campos:
     *
     * rx  = mensajes validos que llegaron a app_handle_message()
     * ae  = errores de aplicacion: tipo inesperado o comando desconocido
     * irq = bytes recibidos por la interrupcion USART1
     * pb  = bytes consumidos por el parser
     * pm  = mensajes validos entregados por el parser
     * pe  = errores detectados por el parser
     * qd  = bytes descartados porque la cola RX estaba llena
     */
    written = snprintf(payload, sizeof(payload), "rx=%lu,ae=%lu,irq=%lu,pb=%lu,pm=%lu,pe=%lu,qd=%lu",
                       (unsigned long) g_rx_count,
                       (unsigned long) g_error_count,
                       (unsigned long) uart_comm_get_rx_irq_count(),
                       (unsigned long) tasks_get_parser_byte_count(),
                       (unsigned long) tasks_get_parser_message_count(),
                       (unsigned long) tasks_get_parser_error_count(),
                       (unsigned long) uart_comm_get_rx_drop_count());

    /*
     * Si los contadores crecieron tanto que el payload no entra en 48 bytes,
     * no dejamos message con basura: mandamos un error explicito y corto.
     */
    if ((written < 0) || ((size_t) written >= sizeof(payload))) {
        (void) protocol_message_set(message, PROTOCOL_TYPE_ERR, "code=status_overflow");
        return;
    }

    /* STS es el tipo de respuesta de estado. */
    (void) protocol_message_set(message, PROTOCOL_TYPE_STS, payload);
}
