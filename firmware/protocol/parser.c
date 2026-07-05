#include "parser.h"

/*
 * Parser incremental del protocolo.
 *
 * "Incremental" significa que no recibimos una trama completa de golpe. La
 * UART entrega bytes de a uno, entonces esta maquina de estados recuerda en
 * que parte de la trama esta parada:
 *
 * WAIT_START              espera '@'
 * READ_LEN_HI/LO          lee los dos digitos hexadecimales de LL
 * EXPECT_LEN_SEPARATOR    espera ':'
 * READ_BODY               junta exactamente LL bytes de "TTT:PAYLOAD"
 * EXPECT_CHECK_SEPARATOR  espera ':'
 * READ_CHECK_HI/LO        lee los dos digitos de CC
 * EXPECT_END              espera '\n' y valida checksum + body
 *
 * Si aparece ruido se descarta la trama parcial. Si el byte que rompio la
 * trama es '@', se reutiliza como comienzo de una trama nueva para resincronizar
 * lo antes posible.
 */


// Resincronización frente al error
static parser_result_t parser_error(parser_t *parser, uint8_t byte)
{
    parser_reset(parser);   //Después del error, descartamos la trama

    if (byte == (uint8_t) PROTOCOL_START_CHAR) {    // Si el byte que rompió la trama es '@'...
        parser->state = PARSER_STATE_READ_LEN_HI;   //...lo reutilizamos como comienzo de una trama nueva
    }   

    return PARSER_RESULT_ERROR; // Informa el fallo para poder llevar el conteo de errores
}

// Convierte dos caracteres hexadecimales en un valor numérico de 8 bits.
static uint8_t parser_hex_byte(const char field[3])
{
    // Cada caracter representa un nibble (0 a 15)
    int8_t hi = hex_char_to_nibble(field[0]);
    int8_t lo = hex_char_to_nibble(field[1]);

    // Desplaza el primer valor 4 bits hacia la izquierda
    // Une el segundo valor en la mitad inferior del byte con un OR bit a bit
    return (uint8_t) (((uint8_t) hi << 4) | (uint8_t) lo);
}


// Valida que el checksum coincida con el calculado.
static bool parser_checksum_matches(const parser_t *parser)
{
    uint8_t expected_checksum = parser_hex_byte(parser->checksum_field);
    uint8_t computed_checksum = 0U;
    uint8_t i;

    /*
     * El checksum se calcula sobre "LL:TTT:PAYLOAD". Como el parser guarda LL
     * y body por separado, reproducimos el XOR en ese mismo orden.
     */

    computed_checksum ^= (uint8_t) parser->length_field[0];
    computed_checksum ^= (uint8_t) parser->length_field[1];
    computed_checksum ^= (uint8_t) PROTOCOL_SEPARATOR_CHAR;
    for (i = 0U; i < parser->expected_body_length; i++) {
        computed_checksum ^= (uint8_t) parser->body[i];
    }

    // Es verdadero o falso que son iguales?
    return computed_checksum == expected_checksum;
}


// Carga el parser con su estado inicial.
void parser_reset(parser_t *parser)
{
    if (parser == NULL) {
        return;
    }   //verifica que el puntero sea válido

    // Vuelve al estado inicial
    parser->state = PARSER_STATE_WAIT_START;

    // Limpia los campos de la trama
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

// Inicializa el parser con su estado inicial.
void parser_init(parser_t *parser)
{
    parser_reset(parser);
}

parser_result_t parser_consume_byte(parser_t *parser, uint8_t byte, protocol_message_t *message)
{
    int8_t nibble;

    if (parser == NULL) {
        return PARSER_RESULT_ERROR;
    }   //verifica que el puntero sea válido

    /*
     * Muchos monitores serie mandan "\r\n". El protocolo cierra con '\n', asi
     * que ignoramos '\r' para que no rompa la trama.
     */
    if (byte == (uint8_t) '\r') {
        return PARSER_RESULT_IN_PROGRESS;
    }

    // Cicla la máquina de estados del parser
    switch (parser->state) {
    case PARSER_STATE_WAIT_START:
        if (byte == (uint8_t) PROTOCOL_START_CHAR) {
            parser->state = PARSER_STATE_READ_LEN_HI;
        }   //Esperábamos el incio y llegó un '@'. Pasamos al siguiente estado.
        return PARSER_RESULT_IN_PROGRESS;

    case PARSER_STATE_READ_LEN_HI:
        nibble = hex_char_to_nibble((char) byte);
        if (nibble < 0) {//Esperamos recibir un caracter hexadecimal.
            return parser_error(parser, byte);  //Si no lo es, error. 
        }    
        parser->length_field[0] = (char) byte;  //Si sí lo es, guardamos y ciclamos.
        parser->state = PARSER_STATE_READ_LEN_LO;
        return PARSER_RESULT_IN_PROGRESS;

    case PARSER_STATE_READ_LEN_LO:
        nibble = hex_char_to_nibble((char) byte);
        if (nibble < 0) {//Esperamos recibir un caracter hexadecimal.
            return parser_error(parser, byte);  //Si no lo es, error.
        }
        parser->length_field[1] = (char) byte;
        parser->length_field[2] = '\0'; //finaliza la cadena "length_field"
        parser->expected_body_length = parser_hex_byte(parser->length_field);
        if ((parser->expected_body_length < (PROTOCOL_TYPE_LENGTH + 1U)) ||
            (parser->expected_body_length > PROTOCOL_MAX_BODY_SIZE)) {
            return parser_error(parser, byte);
        }   //Verifica que la longitud no sea absurda, luego cicla al siguiente estado.
        parser->state = PARSER_STATE_EXPECT_LEN_SEPARATOR;
        return PARSER_RESULT_IN_PROGRESS;

    case PARSER_STATE_EXPECT_LEN_SEPARATOR:
        if (byte != (uint8_t) PROTOCOL_SEPARATOR_CHAR) {
            return parser_error(parser, byte);
        }   //Si recibimos el separador, entonces sigue el body.
        parser->body_index = 0U;//El siguiente estado es el inicio del body (index 0)
        parser->state = PARSER_STATE_READ_BODY;
        return PARSER_RESULT_IN_PROGRESS;

    case PARSER_STATE_READ_BODY:
        parser->body[parser->body_index] = (char) byte;
        parser->body_index++;//Seguimos leyendo body hasta que la longitud esperada.
        if (parser->body_index >= parser->expected_body_length) {
            parser->body[parser->body_index] = '\0';
            parser->state = PARSER_STATE_EXPECT_CHECK_SEPARATOR;
        }   // Si terminó el body, reseteamos el index y ciclamos el estado.
        return PARSER_RESULT_IN_PROGRESS;

    case PARSER_STATE_EXPECT_CHECK_SEPARATOR:
        if (byte != (uint8_t) PROTOCOL_SEPARATOR_CHAR) {
            return parser_error(parser, byte);
        }   //Recibimos el separador, entonces sigue el checksum.
        parser->state = PARSER_STATE_READ_CHECK_HI;
        return PARSER_RESULT_IN_PROGRESS;

    case PARSER_STATE_READ_CHECK_HI:
        nibble = hex_char_to_nibble((char) byte);
        if (nibble < 0) {
            return parser_error(parser, byte);
        }   //Si es válido, lo cargamos y buscamos el siguiente nibble CHECK_LO.
        parser->checksum_field[0] = (char) byte;
        parser->state = PARSER_STATE_READ_CHECK_LO;
        return PARSER_RESULT_IN_PROGRESS;

    case PARSER_STATE_READ_CHECK_LO:
        nibble = hex_char_to_nibble((char) byte);
        if (nibble < 0) {
            return parser_error(parser, byte);
        }   // Cargamos el valor y pasamos al estado final de espera.
        parser->checksum_field[1] = (char) byte;
        parser->checksum_field[2] = '\0';
        parser->state = PARSER_STATE_EXPECT_END;
        return PARSER_RESULT_IN_PROGRESS;

    case PARSER_STATE_EXPECT_END:
        if (byte != (uint8_t) PROTOCOL_END_CHAR) {
            return parser_error(parser, byte);
        }   //Si no es el final de trama, error.
        if ((message == NULL) || !parser_checksum_matches(parser) ||
            !protocol_decode_body(parser->body, parser->expected_body_length, message)) {
            parser_reset(parser);
            return PARSER_RESULT_ERROR;
        }   //Si el mensaje es nulo, tiene checksum incorrecto o no se decodifica, error.
        parser_reset(parser);   //La info ya fue carga a message. Limpiamos el parser
        return PARSER_RESULT_MESSAGE_READY;

    default:
        parser_reset(parser);
        return PARSER_RESULT_ERROR;
    }
}
