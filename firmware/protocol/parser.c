#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "parser.h"

/*
 * Parser incremental del protocolo serie.
 *
 * La USART no entrega "mensajes": entrega bytes. Por eso este modulo guarda el
 * contexto minimo entre interrupciones/tareas y avanza una FSM con cada byte de
 * entrada. LL define cuantos bytes del cuerpo TTT:PAYLOAD se deben juntar antes
 * de mirar el checksum, tal como se destaca en el apunte de protocolo serie.
 */

/* El protocolo acepta hexadecimal en mayusculas para LL y CC. */
static bool is_hex_digit(uint8_t byte)
{
    if ((byte >= (uint8_t) '0') && (byte <= (uint8_t) '9')) {
        return true;
    }

    if ((byte >= (uint8_t) 'A') && (byte <= (uint8_t) 'F')) {
        return true;
    }

    return false;
}

/* Par hexadecimal ASCII -> byte. Los caracteres ya fueron filtrados. */
static uint8_t hex_pair_to_value(const char *text)
{
    const int hi = hex_char_to_nibble(text[0]);
    const int lo = hex_char_to_nibble(text[1]);

    return (uint8_t) (((uint8_t) hi << 4U) | (uint8_t) lo);
}

/* Regla de cuerpo: texto imprimible, sin '@' para no perder resincronismo. */
static bool is_body_byte_allowed(uint8_t byte)
{
    if ((byte < 0x20U) || (byte > 0x7EU)) {
        return false;
    }

    if (byte == (uint8_t) PROTOCOL_START_CHAR) {
        return false;
    }

    return true;
}

/* Error comun: descarta la trama parcial y aplica la regla de reuso de '@'. */
static parser_result_t parser_fail(parser_t *parser, uint8_t byte)
{
    parser_reset(parser);

    /*
     * Si el byte conflictivo es un nuevo inicio, no conviene tirarlo: puede ser
     * el primer byte de la siguiente trama valida despues del ruido.
     */
    if (byte == (uint8_t) PROTOCOL_START_CHAR) {
        parser->state = PARSER_STATE_READ_LEN_HI;
    }

    return PARSER_RESULT_ERROR;
}

/* Estado base: sin trama en curso, esperando el proximo '@'. */
void parser_reset(parser_t *parser)
{
    if (parser == NULL) {
        return;
    }

    parser->state = PARSER_STATE_WAIT_START;

    parser->length_field[0] = '\0';
    parser->length_field[1] = '\0';
    parser->length_field[2] = '\0';

    parser->body[0] = '\0';

    parser->checksum_field[0] = '\0';
    parser->checksum_field[1] = '\0';
    parser->checksum_field[2] = '\0';

    parser->expected_body_length = 0U;
    parser->body_index = 0U;
}

/* Inicializacion publica del parser. */
void parser_init(parser_t *parser)
{
    parser_reset(parser);
}

/* Avanza la FSM con un byte del flujo UART. */
parser_result_t parser_consume_byte(parser_t *parser, uint8_t byte, protocol_message_t *message)
{
    char checksum_input[3U + PROTOCOL_MAX_BODY_SIZE];
    uint8_t received_checksum;
    uint8_t computed_checksum;

    if ((parser == NULL) || (message == NULL)) {
        return PARSER_RESULT_ERROR;
    }

    /*
     * Muchas terminales agregan CR antes de LF. La trama del TP termina con LF,
     * asi que CR no participa del estado ni debe contarse como error.
     */
    if (byte == (uint8_t) '\r') {
        return PARSER_RESULT_IN_PROGRESS;
    }

    switch (parser->state) {
    case PARSER_STATE_WAIT_START:
        if (byte == (uint8_t) PROTOCOL_START_CHAR) {
            parser_reset(parser);
            parser->state = PARSER_STATE_READ_LEN_HI;
        }
        return PARSER_RESULT_IN_PROGRESS;

    case PARSER_STATE_READ_LEN_HI:
        if (!is_hex_digit(byte)) {
            return parser_fail(parser, byte);
        }

        parser->length_field[0] = (char) byte;
        parser->state = PARSER_STATE_READ_LEN_LO;

        return PARSER_RESULT_IN_PROGRESS;

    case PARSER_STATE_READ_LEN_LO:
        if (!is_hex_digit(byte)) {
            return parser_fail(parser, byte);
        }

        parser->length_field[1] = (char) byte;
        parser->length_field[2] = '\0';

        parser->expected_body_length = hex_pair_to_value(parser->length_field);

        /* LL debe cubrir al menos TTT: y no puede superar el buffer fijo. */
        if (parser->expected_body_length < (PROTOCOL_TYPE_LENGTH + 1U)) {
            return parser_fail(parser, byte);
        }

        if (parser->expected_body_length > PROTOCOL_MAX_BODY_SIZE) {
            return parser_fail(parser, byte);
        }

        parser->state = PARSER_STATE_EXPECT_LEN_SEPARATOR;

        return PARSER_RESULT_IN_PROGRESS;

    case PARSER_STATE_EXPECT_LEN_SEPARATOR:
        if (byte != (uint8_t) PROTOCOL_SEPARATOR_CHAR) {
            return parser_fail(parser, byte);
        }

        parser->body_index = 0U;
        parser->state = PARSER_STATE_READ_BODY;

        return PARSER_RESULT_IN_PROGRESS;

    case PARSER_STATE_READ_BODY:
        if (!is_body_byte_allowed(byte)) {
            return parser_fail(parser, byte);
        }

        if (parser->body_index >= PROTOCOL_MAX_BODY_SIZE) {
            return parser_fail(parser, byte);
        }

        parser->body[parser->body_index] = (char) byte;
        parser->body_index++;

        if (parser->body_index == parser->expected_body_length) {
            parser->body[parser->body_index] = '\0';
            parser->state = PARSER_STATE_EXPECT_CHECK_SEPARATOR;
        }

        return PARSER_RESULT_IN_PROGRESS;

    case PARSER_STATE_EXPECT_CHECK_SEPARATOR:
        if (byte != (uint8_t) PROTOCOL_SEPARATOR_CHAR) {
            return parser_fail(parser, byte);
        }

        parser->state = PARSER_STATE_READ_CHECK_HI;

        return PARSER_RESULT_IN_PROGRESS;

    case PARSER_STATE_READ_CHECK_HI:
        if (!is_hex_digit(byte)) {
            return parser_fail(parser, byte);
        }

        parser->checksum_field[0] = (char) byte;
        parser->state = PARSER_STATE_READ_CHECK_LO;

        return PARSER_RESULT_IN_PROGRESS;

    case PARSER_STATE_READ_CHECK_LO:
        if (!is_hex_digit(byte)) {
            return parser_fail(parser, byte);
        }

        parser->checksum_field[1] = (char) byte;
        parser->checksum_field[2] = '\0';

        parser->state = PARSER_STATE_EXPECT_END;

        return PARSER_RESULT_IN_PROGRESS;

    case PARSER_STATE_EXPECT_END:
        if (byte != (uint8_t) PROTOCOL_END_CHAR) {
            return parser_fail(parser, byte);
        }

        /*
         * El checksum protege LL, el separador y el cuerpo. No incluye '@', el
         * ':' previo a CC ni el LF, siguiendo la especificacion del apunte 13.
         */
        checksum_input[0] = parser->length_field[0];
        checksum_input[1] = parser->length_field[1];
        checksum_input[2] = PROTOCOL_SEPARATOR_CHAR;
        memcpy(&checksum_input[3], parser->body, parser->expected_body_length);

        received_checksum = hex_pair_to_value(parser->checksum_field);
        computed_checksum = protocol_compute_checksum(checksum_input,
                                                      (size_t) parser->expected_body_length + 3U);

        if (received_checksum != computed_checksum) {
            parser_reset(parser);
            return PARSER_RESULT_ERROR;
        }

        /* Recien con checksum correcto se separa TTT del payload. */
        if (!protocol_decode_body(parser->body, parser->expected_body_length, message)) {
            parser_reset(parser);
            return PARSER_RESULT_ERROR;
        }

        parser_reset(parser);

        return PARSER_RESULT_MESSAGE_READY;

    default:
        parser_reset(parser);
        return PARSER_RESULT_ERROR;
    }
}
