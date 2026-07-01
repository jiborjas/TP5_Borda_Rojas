#include <string.h>

#include "parser.h"

/*
 * Parser incremental a completar por el alumno.
 *
 * La idea es consumir un byte por vez y recorrer una FSM con estados como:
 * WAIT_START -> READ_LEN_HI -> READ_LEN_LO -> ... -> EXPECT_END
 */

void parser_reset(parser_t *parser)
{
    if (parser == NULL) {
        return;
    }

    parser->state = PARSER_STATE_WAIT_START;
    parser->length_field[0] = '\0';
    parser->length_field[1] = '\0';
    parser->length_field[2] = '\0';
    parser->checksum_field[0] = '\0';
    parser->checksum_field[1] = '\0';
    parser->checksum_field[2] = '\0';
    parser->body[0] = '\0';
    parser->expected_body_length = 0U;
    parser->body_index = 0U;
}

void parser_init(parser_t *parser)
{
    parser_reset(parser);
}

parser_result_t parser_consume_byte(parser_t *parser, uint8_t byte, protocol_message_t *message)
{
    (void) byte;
    (void) message;

    if (parser == NULL) {
        return PARSER_RESULT_ERROR;
    }

    /*
     * TODO(alumno):
     * - ignorar '\r'
     * - detectar inicio '@'
     * - leer longitud hexadecimal LL
     * - validar ':'
     * - acumular body TTT:PAYLOAD
     * - leer checksum CC
     * - validar '\n'
     * - verificar checksum
     * - llamar a protocol_decode_body(...)
     * - devolver PARSER_RESULT_MESSAGE_READY cuando haya mensaje válido
     */

    parser_reset(parser);
    return PARSER_RESULT_ERROR;
}
