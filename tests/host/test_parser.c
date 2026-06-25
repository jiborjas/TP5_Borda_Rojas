#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "parser.h"
#include "protocol.h"

/* Contador de pruebas correctas. */
static int g_ok = 0;

/* Contador de pruebas fallidas. */
static int g_fail = 0;

/* Verifica enteros simples. */
static void expect_int(const char *name, int got, int expected)
{
    /* Si coincide, suma OK. */
    if (got == expected) {
        printf("[OK]   %s -> %d\n", name, got);
        g_ok++;
        return;
    }

    /* Si no coincide, suma fallo. */
    printf("[FAIL] %s -> got %d expected %d\n", name, got, expected);
    g_fail++;
}

/* Verifica strings. */
static void expect_string(const char *name, const char *got, const char *expected)
{
    /* strcmp devuelve 0 cuando ambos strings son iguales. */
    if (strcmp(got, expected) == 0) {
        printf("[OK]   %s -> %s\n", name, got);
        g_ok++;
        return;
    }

    /* Mostramos ambas versiones si no coinciden. */
    printf("[FAIL] %s -> got %s expected %s\n", name, got, expected);
    g_fail++;
}

/* Alimenta el parser con un stream y cuenta mensajes/errores. */
static void feed_stream(const char *stream,
                        int *ready_count,
                        int *error_count,
                        protocol_message_t *last_message)
{
    /* Parser incremental igual al que usa task_parser. */
    parser_t parser;

    /* Resultado de cada byte. */
    parser_result_t result;

    /* Indice dentro del stream de prueba. */
    size_t i;

    /* Estado inicial: esperar '@'. */
    parser_init(&parser);

    /* Contadores de salida. */
    *ready_count = 0;
    *error_count = 0;

    /* Recorremos byte por byte, como si viniera de UART. */
    for (i = 0U; stream[i] != '\0'; i++) {
        /* Consumimos un byte. */
        result = parser_consume_byte(&parser, (uint8_t) stream[i], last_message);

        /* Mensaje completo y valido. */
        if (result == PARSER_RESULT_MESSAGE_READY) {
            (*ready_count)++;
        }

        /* Error de framing, longitud, checksum o formato. */
        if (result == PARSER_RESULT_ERROR) {
            (*error_count)++;
        }
    }
}

/* Valida que un stream entregue exactamente un mensaje esperado. */
static void check_one_message(const char *name,
                              const char *stream,
                              protocol_type_t expected_type,
                              const char *expected_payload,
                              int expected_errors)
{
    /* Mensaje entregado por el parser. */
    protocol_message_t message;

    /* Cantidad de mensajes listos. */
    int ready_count;

    /* Cantidad de errores. */
    int error_count;

    /* Ejecutamos el stream byte a byte. */
    feed_stream(stream, &ready_count, &error_count, &message);

    /* Debe haber exactamente un mensaje valido. */
    expect_int(name, ready_count, 1);

    /* Debe haber la cantidad esperada de errores previos. */
    expect_int("parser errors", error_count, expected_errors);

    /* El tipo debe coincidir. */
    expect_int("message type", (int) message.type, (int) expected_type);

    /* El payload debe coincidir. */
    expect_string("message payload", message.payload, expected_payload);
}

/* Punto de entrada del test de etapa 2. */
int main(void)
{
    /* Mensaje auxiliar para tests generales. */
    protocol_message_t message;

    /* Cantidad de mensajes listos. */
    int ready_count;

    /* Cantidad de errores. */
    int error_count;

    /* Titulo visible en consola. */
    printf("Etapa 2 - parser incremental\n");

    /* Trama normal. */
    check_one_message("valid ping", "@08:CMD:ping:52\n", PROTOCOL_TYPE_CMD, "ping", 0);

    /* Ruido antes de '@' debe ignorarse. */
    check_one_message("noise before frame", "ruido@@08:CMD:ping:52\n", PROTOCOL_TYPE_CMD, "ping", 1);

    /* CR antes de LF debe ignorarse. */
    check_one_message("CRLF frame", "@08:CMD:ping:52\r\n", PROTOCOL_TYPE_CMD, "ping", 0);

    /* Resincronizacion: el segundo '@' se reutiliza como inicio nuevo. */
    check_one_message("resync at @", "@0@08:CMD:ping:52\n", PROTOCOL_TYPE_CMD, "ping", 1);

    /* ACK tambien es un tipo valido para el parser. */
    check_one_message("valid ACK", "@0A:ACK:pong=1:22\n", PROTOCOL_TYPE_ACK, "pong=1", 0);

    /* Checksum incorrecto: no debe entregar mensaje. */
    feed_stream("@08:CMD:ping:53\n", &ready_count, &error_count, &message);
    expect_int("bad checksum ready", ready_count, 0);
    expect_int("bad checksum errors", error_count, 1);

    /* Dos tramas pegadas: el parser debe entregar dos mensajes. */
    feed_stream("@08:CMD:ping:52\n@0A:CMD:led=on:6A\n", &ready_count, &error_count, &message);
    expect_int("two frames ready", ready_count, 2);
    expect_int("two frames errors", error_count, 0);
    expect_string("two frames last payload", message.payload, "led=on");

    /* Longitud corrupta: el parser debe rechazar. */
    feed_stream("@09:CMD:ping:52\n", &ready_count, &error_count, &message);
    expect_int("bad length ready", ready_count, 0);
    expect_int("bad length errors", error_count, 1);

    /* Resumen final. */
    printf("Resultado etapa 2: %d OK, %d FAIL\n", g_ok, g_fail);

    /* El make falla si alguna prueba fallo. */
    return (g_fail == 0) ? 0 : 1;
}
