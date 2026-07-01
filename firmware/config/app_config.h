#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#define UART_BAUDRATE                 115200U

#define UART_RX_ISR_QUEUE_LENGTH      64U
#define PARSER_INPUT_QUEUE_LENGTH     64U
#define APP_MESSAGE_QUEUE_LENGTH      8U
#define UART_TX_QUEUE_LENGTH          8U
#define ACTUATOR_QUEUE_LENGTH         8U

#define TELEMETRY_PERIOD_MS           1000U
#define STATUS_PERIOD_MS              5000U

#define PROTOCOL_MAX_PAYLOAD_LENGTH   48U
#define PROTOCOL_MAX_FRAME_LENGTH     64U

/*
 * El protocolo define '\n' como fin de trama. Algunos monitores serie, sin
 * embargo, necesitan ver "\r\n" para mostrar cada linea desde la columna 0.
 * Si esta opcion esta activa, la tarea UART TX cambia el ultimo '\n' por
 * "\r\n" solo al transmitir por UART. El encoder del protocolo sigue generando
 * tramas puras con '\n'.
 */
#define UART_TX_CRLF_FOR_TERMINAL      1U

#endif
