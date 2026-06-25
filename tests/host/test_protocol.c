#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "protocol.h"

/* Contador de pruebas correctas. */
static int g_ok = 0;

/* Contador de pruebas fallidas. */
static int g_fail = 0;

/* Verifica enteros simples. */
static void expect_int(const char *name, int got, int expected)
{
    /* Caso feliz: coincide. */
    if (got == expected) {
        printf("[OK]   %s -> %d\n", name, got);
        g_ok++;
        return;
    }

    /* Caso fallido: mostramos ambos valores. */
    printf("[FAIL] %s -> got %d expected %d\n", name, got, expected);
    g_fail++;
}

/* Verifica bytes impresos como hexadecimal. */
static void expect_hex8(const char *name, uint8_t got, uint8_t expected)
{
    /* Caso feliz: coincide. */
    if (got == expected) {
        printf("[OK]   %s -> 0x%02X\n", name, got);
        g_ok++;
        return;
    }

    /* Caso fallido: mostramos ambos checksums. */
    printf("[FAIL] %s -> got 0x%02X expected 0x%02X\n", name, got, expected);
    g_fail++;
}

/* Verifica strings, incluyendo tramas con '\n'. */
static void expect_string(const char *name, const char *got, const char *expected)
{
    /* strcmp devuelve cero si ambos textos son iguales. */
    if (strcmp(got, expected) == 0) {
        printf("[OK]   %s -> %s", name, got);
        g_ok++;
        return;
    }

    /* Si falla, mostramos las dos versiones. */
    printf("[FAIL] %s\n       got      %s       expected %s", name, got, expected);
    g_fail++;
}

/* Prueba protocol_encode() contra una trama esperada. */
static void check_encode(const char *type, const char *payload, const char *expected)
{
    /* Buffer donde se arma @LL:TTT:PAYLOAD:CC\n. */
    char frame[PROTOCOL_MAX_FRAME_SIZE];

    /* Cantidad de caracteres escritos por protocol_encode(). */
    int written;

    /* Limpiamos el buffer para que sea facil ver errores de terminador. */
    memset(frame, 0, sizeof(frame));

    /* Ejecutamos la funcion de la etapa 1. */
    written = protocol_encode(type, payload, frame, sizeof(frame));

    /* Verificamos longitud sin contar '\0'. */
    expect_int("protocol_encode length", written, (int) strlen(expected));

    /* Verificamos trama completa. */
    expect_string("protocol_encode frame", frame, expected);
}

/* Prueba la API usada por task_uart_tx(). */
static void check_encode_frame_api(void)
{
    /* Mensaje logico antes de serializar. */
    protocol_message_t message;

    /* Trama de salida. */
    char frame[PROTOCOL_MAX_FRAME_SIZE];

    /* Longitud real de salida. */
    size_t frame_length = 0U;

    /* Armamos CMD:ping. */
    bool ok = protocol_message_set(&message, PROTOCOL_TYPE_CMD, "ping");

    /* Debe poder crearse. */
    expect_int("protocol_message_set CMD ping", ok ? 1 : 0, 1);

    /* Codificamos usando la API del firmware. */
    ok = protocol_encode_frame(&message, frame, sizeof(frame), &frame_length);

    /* Debe codificar correctamente. */
    expect_int("protocol_encode_frame ok", ok ? 1 : 0, 1);

    /* La longitud debe coincidir con la trama conocida. */
    expect_int("protocol_encode_frame length", (int) frame_length, (int) strlen("@08:CMD:ping:52\n"));

    /* El texto debe coincidir con la tabla del PDF. */
    expect_string("protocol_encode_frame text", frame, "@08:CMD:ping:52\n");
}

/* Punto de entrada del test de etapa 1. */
int main(void)
{
    /* Titulo visible en consola. */
    printf("Etapa 1 - framing, checksum y decode\n");

    /* Conversion hexadecimal basica. */
    expect_int("hex_char_to_nibble('0')", hex_char_to_nibble('0'), 0);
    expect_int("hex_char_to_nibble('9')", hex_char_to_nibble('9'), 9);
    expect_int("hex_char_to_nibble('A')", hex_char_to_nibble('A'), 10);
    expect_int("hex_char_to_nibble('F')", hex_char_to_nibble('F'), 15);
    expect_int("hex_char_to_nibble('a')", hex_char_to_nibble('a'), -1);

    /* Checksums de referencia del PDF. */
    expect_hex8("checksum 08:CMD:ping",
                protocol_checksum("08:CMD:ping", strlen("08:CMD:ping")),
                0x52U);
    expect_hex8("checksum 0E:CMD:led=toggle",
                protocol_checksum("0E:CMD:led=toggle", strlen("0E:CMD:led=toggle")),
                0x7DU);

    /* Tramas principales de la consigna. */
    check_encode("CMD", "ping", "@08:CMD:ping:52\n");
    check_encode("CMD", "led=on", "@0A:CMD:led=on:6A\n");
    check_encode("CMD", "led=off", "@0B:CMD:led=off:07\n");
    check_encode("CMD", "led=toggle", "@0E:CMD:led=toggle:7D\n");
    check_encode("CMD", "status?", "@0B:CMD:status?:13\n");
    check_encode("ACK", "pong=1", "@0A:ACK:pong=1:22\n");
    check_encode("ACK", "cmd=ok", "@0A:ACK:cmd=ok:6B\n");
    check_encode("ERR", "code=unknown_cmd", "@14:ERR:code=unknown_cmd:2D\n");

    /* Validacion sin '@' ni '\n', tal como pide protocol_validate(). */
    expect_int("validate ping ok", protocol_validate("08:CMD:ping:52", strlen("08:CMD:ping:52")), 0);
    expect_int("validate bad checksum", protocol_validate("08:CMD:ping:53", strlen("08:CMD:ping:53")), -1);
    expect_int("validate bad length", protocol_validate("09:CMD:ping:52", strlen("09:CMD:ping:52")), -1);
    expect_int("validate bad type", protocol_validate("08:ABC:ping:58", strlen("08:ABC:ping:58")), -1);

    /* API usada por el firmware integrado. */
    check_encode_frame_api();

    /* Resumen final. */
    printf("Resultado etapa 1: %d OK, %d FAIL\n", g_ok, g_fail);

    /* El make falla si alguna prueba fallo. */
    return (g_fail == 0) ? 0 : 1;
}
