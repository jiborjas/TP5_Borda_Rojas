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

static QueueHandle_t g_uart_rx_isr_queue;
static QueueHandle_t g_parser_input_queue;
static QueueHandle_t g_app_queue;
static QueueHandle_t g_uart_tx_queue;
static QueueHandle_t g_actuator_queue;
static volatile uint32_t g_parser_byte_count = 0U;
static volatile uint32_t g_parser_message_count = 0U;
static volatile uint32_t g_parser_error_count = 0U;

static void task_uart_rx(void *args)
{
    uint8_t byte = 0U;
    (void) args;

    while (1) {
        if (xQueueReceive(g_uart_rx_isr_queue, &byte, portMAX_DELAY) == pdPASS) {
            xQueueSend(g_parser_input_queue, &byte, portMAX_DELAY);
        }
    }
}

static void task_parser(void *args)
{
    uint8_t byte = 0U;
    parser_t parser;
    protocol_message_t message;
    parser_result_t result;
    (void) args;

    parser_init(&parser);

    while (1) {
        if (xQueueReceive(g_parser_input_queue, &byte, portMAX_DELAY) != pdPASS) {
            continue;
        }

        g_parser_byte_count++;
        result = parser_consume_byte(&parser, byte, &message);
        if (result == PARSER_RESULT_MESSAGE_READY) {
            g_parser_message_count++;
            xQueueSend(g_app_queue, &message, portMAX_DELAY);
        } else if (result == PARSER_RESULT_ERROR) {
            g_parser_error_count++;
        }
    }
}

uint32_t tasks_get_parser_byte_count(void)
{
    return g_parser_byte_count;
}

uint32_t tasks_get_parser_message_count(void)
{
    return g_parser_message_count;
}

uint32_t tasks_get_parser_error_count(void)
{
    return g_parser_error_count;
}

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

static void task_telemetry(void *args)
{
    TickType_t last_wake = xTaskGetTickCount();
    protocol_message_t message;
    uint32_t counter = 0U;
    (void) args;

    while (1) {
        if (app_build_telemetry_message(&message, counter)) {
            xQueueSend(g_uart_tx_queue, &message, portMAX_DELAY);
        }

        if ((counter % (STATUS_PERIOD_MS / TELEMETRY_PERIOD_MS)) == 0U) {
            app_build_status_message(&message);
            xQueueSend(g_uart_tx_queue, &message, portMAX_DELAY);
        }

        counter++;
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(TELEMETRY_PERIOD_MS));
    }
}

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

static void task_uart_tx(void *args)
{
    protocol_message_t message;
    char frame[PROTOCOL_MAX_FRAME_SIZE];
    size_t frame_length = 0U;
    (void) args;

    while (1) {
        if (xQueueReceive(g_uart_tx_queue, &message, portMAX_DELAY) != pdPASS) {
            continue;
        }

        if (protocol_encode_frame(&message, frame, sizeof(frame), &frame_length)) {
            uart_comm_send_bytes((const uint8_t *) frame, frame_length);
        }
    }
}

void tasks_start(void)
{
    g_uart_rx_isr_queue = xQueueCreate(UART_RX_ISR_QUEUE_LENGTH, sizeof(uint8_t));
    g_parser_input_queue = xQueueCreate(PARSER_INPUT_QUEUE_LENGTH, sizeof(uint8_t));
    g_app_queue = xQueueCreate(APP_MESSAGE_QUEUE_LENGTH, sizeof(protocol_message_t));
    g_uart_tx_queue = xQueueCreate(UART_TX_QUEUE_LENGTH, sizeof(protocol_message_t));
    g_actuator_queue = xQueueCreate(ACTUATOR_QUEUE_LENGTH, sizeof(actuator_command_t));

    configASSERT(g_uart_rx_isr_queue != NULL);
    configASSERT(g_parser_input_queue != NULL);
    configASSERT(g_app_queue != NULL);
    configASSERT(g_uart_tx_queue != NULL);
    configASSERT(g_actuator_queue != NULL);

    uart_comm_set_rx_queue(g_uart_rx_isr_queue);

    xTaskCreate(task_uart_rx, "uart_rx", 160, NULL, 3, NULL);
    xTaskCreate(task_parser, "parser", 192, NULL, 3, NULL);
    xTaskCreate(task_app, "app", 192, NULL, 2, NULL);
    xTaskCreate(task_telemetry, "telemetry", 192, NULL, 2, NULL);
    xTaskCreate(task_actuators, "actuators", 160, NULL, 2, NULL);
    xTaskCreate(task_uart_tx, "uart_tx", 192, NULL, 2, NULL);

    vTaskStartScheduler();
    configASSERT(false);
}
