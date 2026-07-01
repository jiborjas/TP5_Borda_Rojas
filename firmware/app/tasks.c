#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "app.h"
#include "actuators.h"
#include "sensors.h"
#include "tasks.h"
#include "../config/app_config.h"
#include "../drivers/uart_comm.h"
#include "../protocol/parser.h"
#include "../protocol/protocol.h"

/*
/* Integracion FreeRTOS.
 *
 * La ISR solo encola bytes, una tarea consume el flujo con el parser, otra
 * ejecuta la aplicacion y una tarea dedicada serializa las respuestas por USART1.
 */

/* Cola donde la ISR deja bytes apenas llegan por USART1. */
static QueueHandle_t g_uart_rx_isr_queue;

/* Cola intermedia: task_uart_rx le pasa bytes a task_parser. */
static QueueHandle_t g_parser_input_queue;

/* Cola de mensajes ya validados hacia la aplicacion. */
static QueueHandle_t g_app_queue;

/* Cola de mensajes logicos que deben salir por UART. */
static QueueHandle_t g_uart_tx_queue;

/* Cola de ordenes para actuadores. */
static QueueHandle_t g_actuator_queue;

/* pb del status: cantidad de bytes que entro al parser. */
static volatile uint32_t g_parser_byte_count = 0U;

/* pm del status: cantidad de mensajes validos entregados por el parser. */
static volatile uint32_t g_parser_message_count = 0U;

/* pe del status: cantidad de errores detectados por el parser. */
static volatile uint32_t g_parser_error_count = 0U;

/* Tarea que mueve bytes desde la cola de ISR hacia la cola del parser. */
static void task_uart_rx(void *args)
{
    /* Byte recibido desde USART1. */
    uint8_t byte = 0U;

    /* No usamos parametros de tarea. */
    (void) args;

    /* Bucle permanente de la tarea. */
    while (1) {
        /* Esperamos hasta que la ISR deje un byte en la cola. */
        if (xQueueReceive(g_uart_rx_isr_queue, &byte, portMAX_DELAY) == pdPASS) {
            /* Pasamos el byte a la tarea parser. */
            xQueueSend(g_parser_input_queue, &byte, portMAX_DELAY);
        }
    }
}

/* Tarea que ejecuta el parser byte a byte. */
static void task_parser(void *args)
{
    /* Byte actual que llega desde task_uart_rx. */
    uint8_t byte = 0U;

    /* Estado completo del parser incremental. */
    parser_t parser;

    /* Mensaje de salida cuando una trama se completa. */
    protocol_message_t message;

    /* Resultado de consumir un byte. */
    parser_result_t result;

    /* No usamos parametros de tarea. */
    (void) args;

    /* La FSM arranca esperando '@'. */
    parser_init(&parser);

    /* Bucle permanente del parser. */
    while (1) {
        /* Esperamos el siguiente byte crudo. */
        if (xQueueReceive(g_parser_input_queue, &byte, portMAX_DELAY) != pdPASS) {
            continue;
        }

        /* Contador pb: todo byte que el parser intenta consumir. */
        g_parser_byte_count++;

        /* El parser decide si falta mas, si hubo error o si hay mensaje listo. */
        result = parser_consume_byte(&parser, byte, &message);

        /* Una trama valida se manda a la aplicacion. */
        if (result == PARSER_RESULT_MESSAGE_READY) {
            g_parser_message_count++;
            xQueueSend(g_app_queue, &message, portMAX_DELAY);
        } else if (result == PARSER_RESULT_ERROR) {
            /* Un error se cuenta para status?. */
            g_parser_error_count++;
        }
    }
}

/* Getter usado por app_build_status_message(). */
uint32_t tasks_get_parser_byte_count(void)
{
    /* volatile evita que el compilador asuma que nunca cambia. */
    return g_parser_byte_count;
}

/* Getter usado por app_build_status_message(). */
uint32_t tasks_get_parser_message_count(void)
{
    /* Devuelve pm. */
    return g_parser_message_count;
}

/* Getter usado por app_build_status_message(). */
uint32_t tasks_get_parser_error_count(void)
{
    /* Devuelve pe. */
    return g_parser_error_count;
}

/* Tarea de aplicacion: interpreta CMD:ping, CMD:led=on, etc. */
static void task_app(void *args)
{
    /* Mensaje ya validado por el parser. */
    protocol_message_t message;

    /* No usamos parametros de tarea. */
    (void) args;

    /* Bucle permanente de aplicacion. */
    while (1) {
        /* Esperamos un mensaje completo. */
        if (xQueueReceive(g_app_queue, &message, portMAX_DELAY) == pdPASS) {
            /* La app puede responder por TX y mandar ordenes al LED. */
            app_handle_message(&message, g_uart_tx_queue, g_actuator_queue);
        }
    }
}

/* Tarea periodica: genera DAT y STS. */
static void task_telemetry(void *args)
{
    /* Tick de referencia para vTaskDelayUntil(). */
    TickType_t last_wake = xTaskGetTickCount();

    /* Mensaje reutilizado para DAT y STS. */
    protocol_message_t message;

    /* Secuencia de telemetria simulada. */
    uint32_t counter = 0U;

    /* No usamos parametros de tarea. */
    (void) args;

    /* Bucle permanente de telemetria. */
    while (1) {
        /* Armamos un DAT tipo temp=N,seq=N. */
        if (app_build_telemetry_message(&message, counter)) {
            /* El TX se hace en otra tarea. */
            xQueueSend(g_uart_tx_queue, &message, portMAX_DELAY);
        }

        /* Cada STATUS_PERIOD_MS tambien enviamos STS. */
        if ((counter % (STATUS_PERIOD_MS / TELEMETRY_PERIOD_MS)) == 0U) {
            app_build_status_message(&message);
            xQueueSend(g_uart_tx_queue, &message, portMAX_DELAY);
        }

        /* Incrementamos la secuencia para la proxima telemetria. */
        counter++;

        /* Espera periodica estable: evita drift acumulado. */
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(TELEMETRY_PERIOD_MS));
    }
}

/* Tarea que aplica comandos a los actuadores. */
static void task_actuators(void *args)
{
    /* Orden simple: target="led", action="on/off/toggle". */
    actuator_command_t command;

    /* No usamos parametros de tarea. */
    (void) args;

    /* Bucle permanente de actuadores. */
    while (1) {
        /* Esperamos una orden enviada por app_handle_message(). */
        if (xQueueReceive(g_actuator_queue, &command, portMAX_DELAY) == pdPASS) {
            /* actuators.c conoce el pin PC13 y su logica activo-bajo. */
            actuators_apply_command(&command);
        }
    }
}

/* Tarea unica de transmision UART. */
static void task_uart_tx(void *args)
{
    /* Mensaje logico a transmitir. */
    protocol_message_t message;

    /* Buffer donde entra @LL:TTT:PAYLOAD:CC\n. */
    char frame[PROTOCOL_MAX_FRAME_SIZE];

    /* Longitud real de la trama, sin depender de '\0'. */
    size_t frame_length = 0U;

    /* No usamos parametros de tarea. */
    (void) args;

    /* Bucle permanente de TX. */
    while (1) {
        /* Esperamos un mensaje para enviar. */
        if (xQueueReceive(g_uart_tx_queue, &message, portMAX_DELAY) != pdPASS) {
            continue;
        }

        /* Codificamos el mensaje a bytes UART. */
        if (protocol_encode_frame(&message, frame, sizeof(frame), &frame_length)) {
            /* Enviamos exactamente frame_length bytes por USART1. */
            uart_comm_send_bytes((const uint8_t *) frame, frame_length);
        }
    }
}

/* Crea colas, crea tareas y arranca FreeRTOS. */
void tasks_start(void)
{
    /* Cola usada directamente desde la ISR de USART1. */
    g_uart_rx_isr_queue = xQueueCreate(UART_RX_ISR_QUEUE_LENGTH, sizeof(uint8_t));

    /* Cola que desacopla recepcion de parseo. */
    g_parser_input_queue = xQueueCreate(PARSER_INPUT_QUEUE_LENGTH, sizeof(uint8_t));

    /* Cola de mensajes completos hacia app. */
    g_app_queue = xQueueCreate(APP_MESSAGE_QUEUE_LENGTH, sizeof(protocol_message_t));

    /* Cola de mensajes que deben transmitirse. */
    g_uart_tx_queue = xQueueCreate(UART_TX_QUEUE_LENGTH, sizeof(protocol_message_t));

    /* Cola de ordenes hacia actuadores. */
    g_actuator_queue = xQueueCreate(ACTUATOR_QUEUE_LENGTH, sizeof(actuator_command_t));

    /* Si alguna cola fallo, detenemos en assert para debug. */
    configASSERT(g_uart_rx_isr_queue != NULL);
    configASSERT(g_parser_input_queue != NULL);
    configASSERT(g_app_queue != NULL);
    configASSERT(g_uart_tx_queue != NULL);
    configASSERT(g_actuator_queue != NULL);

    /* Le damos al driver UART la cola donde debe dejar bytes desde ISR. */
    uart_comm_set_rx_queue(g_uart_rx_isr_queue);

    /* RX y parser tienen prioridad mayor para vaciar bytes rapido. */
    xTaskCreate(task_uart_rx, "uart_rx", 160, NULL, 3, NULL);
    xTaskCreate(task_parser, "parser", 192, NULL, 3, NULL);

    /* Aplicacion y salidas corren con prioridad media. */
    xTaskCreate(task_app, "app", 192, NULL, 2, NULL);
    xTaskCreate(task_telemetry, "telemetry", 192, NULL, 2, NULL);
    xTaskCreate(task_actuators, "actuators", 160, NULL, 2, NULL);
    xTaskCreate(task_uart_tx, "uart_tx", 192, NULL, 2, NULL);

    /* Desde aca FreeRTOS toma el control del programa. */
    vTaskStartScheduler();

    /* Si volvemos de vTaskStartScheduler(), algo fallo al arrancar el scheduler. */
    configASSERT(false);
}
