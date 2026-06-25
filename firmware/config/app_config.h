#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* Baudrate pedido por el PDF: 115200 8N1. */
#define UART_BAUDRATE                 115200U

/* Bytes que puede guardar la cola que escribe la ISR de USART1. */
#define UART_RX_ISR_QUEUE_LENGTH      64U

/* Bytes que espera task_parser desde task_uart_rx. */
#define PARSER_INPUT_QUEUE_LENGTH     64U

/* Mensajes completos que pueden esperar a app_handle_message(). */
#define APP_MESSAGE_QUEUE_LENGTH      8U

/* Mensajes logicos que pueden esperar a ser transmitidos por UART. */
#define UART_TX_QUEUE_LENGTH          8U

/* Ordenes pendientes hacia actuators.c. */
#define ACTUATOR_QUEUE_LENGTH         8U

/* Periodo de telemetria DAT en milisegundos. */
#define TELEMETRY_PERIOD_MS           1000U

/* Periodo de estado STS en milisegundos. */
#define STATUS_PERIOD_MS              5000U

/* Payload maximo pedido por la consigna. */
#define PROTOCOL_MAX_PAYLOAD_LENGTH   48U

/* Frame maximo suficiente para @LL:TTT:PAYLOAD:CC\n. */
#define PROTOCOL_MAX_FRAME_LENGTH     64U

#endif
