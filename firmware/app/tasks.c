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

static QueueHandle_t g_uart_rx_isr_queue;	// FiFo de bytes que deja la ISR de UART
static QueueHandle_t g_parser_input_queue;	// FiFo de bytes para que lea el parser
static QueueHandle_t g_app_queue;			// FiFo de protocol_message_t para que lea la app
static QueueHandle_t g_uart_tx_queue;		// FiFo de mensajes para enviar por UART
static QueueHandle_t g_actuator_queue;		// FiFo de comandos para los actuadores

static volatile uint32_t g_parser_byte_count = 0U;		//Contador de bytes recibidos por el parser
static volatile uint32_t g_parser_message_count = 0U;	//Contador de mensajes completos recibidos por el parser
static volatile uint32_t g_parser_error_count = 0U;		//Contador de errores de parseo


// Transporta los bytes de la fila de la ISR de UART a la cola del parser.
static void task_uart_rx(void *args)
{
    uint8_t byte = 0U;
    (void) args;

	//Duerme a la tarea hasta que llegue un byte por la cola de la ISR de UART
    while (1) { 
        if (xQueueReceive(g_uart_rx_isr_queue, &byte, portMAX_DELAY) == pdPASS) {
            xQueueSend(g_parser_input_queue, &byte, portMAX_DELAY);
        }   // La ISR de UART debe ser muy corta: solo deja bytes en una cola.
    }       // Esta tarea toma esos bytes y los pasa a la cola del parser.
}


// Junta los bytes que llegan al parser hasta formar el mensaje completo.
// Recibe desde la cola de bytes del parser y entrega protocol_message_t a la cola de la app.
// Actualiza contadores de bytes, mensajes y errores.
static void task_parser(void *args)
{
    uint8_t byte = 0U;
    parser_t parser;
    protocol_message_t message;
    parser_result_t result;
    (void) args;

    parser_init(&parser);

	//Duerme a la tarea hasta que llegue un byte por la cola del parser
    while (1) {
        if (xQueueReceive(g_parser_input_queue, &byte, portMAX_DELAY) != pdPASS) {
            continue;	
        }//Si llegara a volver con un error y no con un byte, iteraría desde el inicio del while

        g_parser_byte_count++;	//Recibí un byte

        result = parser_consume_byte(&parser, byte, &message);	//Actualiza el estado del parser 

        if (result == PARSER_RESULT_MESSAGE_READY) {
            g_parser_message_count++;	//Recibí un mensaje completo
            xQueueSend(g_app_queue, &message, portMAX_DELAY);
        } 
		else if (result == PARSER_RESULT_ERROR) {
            g_parser_error_count++;		//Recibí un error de parseo
        }
    }
}


// Devuelven copias de los contadores para funciones que están fuera del dominio 
// Por ejemplo, app_build_status_message() 
uint32_t tasks_get_parser_byte_count(void){return g_parser_byte_count;}
uint32_t tasks_get_parser_message_count(void){return g_parser_message_count;}
uint32_t tasks_get_parser_error_count(void){return g_parser_error_count;}


// Recibe protocol_message_t de la cola de la app y llama a app_handle_message().
static void task_app(void *args)
{
    protocol_message_t message;
    (void) args;

    while (1) {
        if (xQueueReceive(g_app_queue, &message, portMAX_DELAY) == pdPASS) {
            app_handle_message(&message, g_uart_tx_queue, g_actuator_queue);
        }
    }
}


//Arma mensajes desde la bluepill y los pone en la cola de salida (Tx) de UART.
static void task_telemetry(void *args)
{
    TickType_t last_wake = xTaskGetTickCount();	
    protocol_message_t message;
    uint32_t counter = 0U;	//para hacer un divisor de frecuencia
    (void) args;

    while (1) {
        if (app_build_telemetry_message(&message, counter)) {
            xQueueSend(g_uart_tx_queue, &message, portMAX_DELAY);
        }	//pone en la cola de Tx un msg de telemetría básica (DAT)

		// divisor de frecuencia para que el reporte de estado (STS) 
		// se envíe con menos frecuencia que la telemetría básica (DAT).
        if ((counter % (STATUS_PERIOD_MS / TELEMETRY_PERIOD_MS)) == 0U) {
            app_build_status_message(&message);
            xQueueSend(g_uart_tx_queue, &message, portMAX_DELAY);
        }
        counter++;

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(TELEMETRY_PERIOD_MS));
    }	//Arma mensajes periodicamente pero SIN busy wait
}


// Recibe actuator_command_t de la FiFo que actualiza app_handle_message() 
// y llama a actuators_apply_command().
static void task_actuators(void *args)
{
    actuator_command_t command;
    (void) args;

    while (1) {
        if (xQueueReceive(g_actuator_queue, &command, portMAX_DELAY) == pdPASS) {
            actuators_apply_command(&command);
        }
    }
}


// Recibe protocol_message_t de la cola de salida (Tx) de UART y lo envía por USART1.
static void task_uart_tx(void *args)
{
    protocol_message_t message;
    char frame[PROTOCOL_MAX_FRAME_SIZE];
    size_t frame_length = 0U;
    (void) args;

    while (1) {
        if (xQueueReceive(g_uart_tx_queue, &message, portMAX_DELAY) != pdPASS) {
            continue;
        }	//Si llegara a volver con un error, iteraría desde el inicio del while

        if (protocol_encode_frame(&message, frame, sizeof(frame), &frame_length)) {
/*
* Todas las respuestas salen por el mismo camino: mensaje en cola,
* encode del protocolo y bytes por USART1.
*
* Para que la salida no se vea "en diagonal" en monitores serie
* simples, enviamos "\r\n" en lugar del ultimo '\n'. El protocolo
* sigue siendo compatible porque el parser ignora '\r'.
*/
#if UART_TX_CRLF_FOR_TERMINAL
            if ((frame_length > 0U) && (frame[frame_length - 1U] == '\n')) {
                const uint8_t crlf[] = {'\r', '\n'};

                uart_comm_send_bytes((const uint8_t *) frame, frame_length - 1U);
                uart_comm_send_bytes(crlf, sizeof(crlf));
                continue;
            }
#endif

            uart_comm_send_bytes((const uint8_t *) frame, frame_length);
        }
    }
}


// Inicializa las FiFo, los thread o tareas y el scheduler de FreeRTOS.
// No debería retornar nunca, porque el scheduler toma el control del CPU.  
void tasks_start(void)
{
	// Crea las colas de comunicación entre threads / tareas y la ISR de UART
    g_uart_rx_isr_queue = xQueueCreate(UART_RX_ISR_QUEUE_LENGTH, sizeof(uint8_t));
    g_parser_input_queue = xQueueCreate(PARSER_INPUT_QUEUE_LENGTH, sizeof(uint8_t));
    g_app_queue = xQueueCreate(APP_MESSAGE_QUEUE_LENGTH, sizeof(protocol_message_t));
    g_uart_tx_queue = xQueueCreate(UART_TX_QUEUE_LENGTH, sizeof(protocol_message_t));
    g_actuator_queue = xQueueCreate(ACTUATOR_QUEUE_LENGTH, sizeof(actuator_command_t));

	// Verifica que las colas se hayan creado correctamente
    configASSERT(g_uart_rx_isr_queue != NULL);
    configASSERT(g_parser_input_queue != NULL);
    configASSERT(g_app_queue != NULL);
    configASSERT(g_uart_tx_queue != NULL);
    configASSERT(g_actuator_queue != NULL);

	// Inicializa la cola de recepción de la ISR de UART
    uart_comm_set_rx_queue(g_uart_rx_isr_queue);

	// Crea las tareas del sistema
    xTaskCreate(task_uart_rx, "uart_rx", 160, NULL, 3, NULL);
    xTaskCreate(task_parser, "parser", 192, NULL, 3, NULL);
    xTaskCreate(task_app, "app", 192, NULL, 2, NULL);
    xTaskCreate(task_telemetry, "telemetry", 192, NULL, 2, NULL);
    xTaskCreate(task_actuators, "actuators", 160, NULL, 2, NULL);
    xTaskCreate(task_uart_tx, "uart_tx", 192, NULL, 2, NULL);

    vTaskStartScheduler();	// Inicia el scheduler de FreeRTOS, que nunca debería retornar
    configASSERT(false);	// Si llega hasta acá, es porque el scheduler no pudo iniciar
}
