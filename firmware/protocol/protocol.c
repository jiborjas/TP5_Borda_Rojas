#include <stdio.h>
#include <string.h>

#include "protocol.h"

/*
 * Protocolo del TP5.
 *
 * Una trama completa tiene esta forma:
 *
 *     @LL:TTT:PAYLOAD:CC\n
 *
 * @       marca el comienzo.
 * LL      son dos digitos hexadecimales con la longitud de "TTT:PAYLOAD".
 * TTT     es el tipo de mensaje, por ejemplo CMD, ACK, STS o ERR.
 * PAYLOAD es el texto del mensaje.
 * CC      es el checksum XOR calculado sobre "LL:TTT:PAYLOAD".
 * \n      marca el final.
 *
 * Este archivo implementa solamente el armado y desarmado de mensajes. El
 * parser incremental byte a byte se implementa aparte en parser.c.
 */

const char *protocol_type_to_text(protocol_type_t type)
{
    /*
     * El firmware trabaja internamente con un enum, porque es mas seguro que
     * comparar strings por todos lados. Esta funcion lo convierte al texto de
     * tres letras que viaja en la UART.
     */
    switch (type) {
    case PROTOCOL_TYPE_CMD:
        return "CMD";
    case PROTOCOL_TYPE_DAT:
        return "DAT";
    case PROTOCOL_TYPE_EVT:
        return "EVT";
    case PROTOCOL_TYPE_STS:
        return "STS";
    case PROTOCOL_TYPE_ACK:
        return "ACK";
    case PROTOCOL_TYPE_ERR:
        return "ERR";
    default:
        return "INV";
    }
}

protocol_type_t protocol_type_from_text(const char *text)
{
    if (text == NULL) {
        return PROTOCOL_TYPE_INVALID;
    }

    /*
     * Solo comparamos los primeros 3 caracteres porque el campo TTT siempre
     * mide PROTOCOL_TYPE_LENGTH. El parser/decoder valida el ':' que viene
     * despues.
     */
    if (strncmp(text, "CMD", PROTOCOL_TYPE_LENGTH) == 0) {
        return PROTOCOL_TYPE_CMD;
    }
    if (strncmp(text, "DAT", PROTOCOL_TYPE_LENGTH) == 0) {
        return PROTOCOL_TYPE_DAT;
    }
    if (strncmp(text, "EVT", PROTOCOL_TYPE_LENGTH) == 0) {
        return PROTOCOL_TYPE_EVT;
    }
    if (strncmp(text, "STS", PROTOCOL_TYPE_LENGTH) == 0) {
        return PROTOCOL_TYPE_STS;
    }
    if (strncmp(text, "ACK", PROTOCOL_TYPE_LENGTH) == 0) {
        return PROTOCOL_TYPE_ACK;
    }
    if (strncmp(text, "ERR", PROTOCOL_TYPE_LENGTH) == 0) {
        return PROTOCOL_TYPE_ERR;
    }

    return PROTOCOL_TYPE_INVALID;
}


// Carga un mensaje con los datos a enviar: Type, payload y length.
bool protocol_message_set(protocol_message_t *message, protocol_type_t type, const char *payload)
{
    size_t payload_length;

    /*
     * Esta funcion arma un protocol_message_t en memoria. Todavia no hay
     * framing ni checksum: solo guardamos tipo, payload y longitud para que
     * protocol_encode_frame() pueda convertirlo luego a bytes UART.
     */
    if ((message == NULL) || (payload == NULL)) {
        return false;
    }
    if (protocol_type_to_text(type)[0] == 'I') {
        return false;
    }

    payload_length = strlen(payload);
    if (payload_length > PROTOCOL_MAX_PAYLOAD_LENGTH) {
        return false;
    }

    message->type = type;
    message->payload_length = (uint8_t) payload_length;
    memcpy(message->payload, payload, payload_length + 1U);
    return true;
}

int8_t hex_char_to_nibble(char c)
{
    /*
     * Un "nibble" son 4 bits, o sea un digito hexadecimal. Esta funcion se usa
     * para convertir caracteres como 'A' o 'f' a su valor numerico 10 o 15.
     * Devuelve -1 si el caracter no es hexadecimal.
     */
    if ((c >= '0') && (c <= '9')) {
        return (int8_t) (c - '0');
    }
    if ((c >= 'A') && (c <= 'F')) {
        return (int8_t) (c - 'A' + 10);
    }
    if ((c >= 'a') && (c <= 'f')) {
        return (int8_t) (c - 'a' + 10);
    }

    return -1;
}

static char nibble_to_hex(uint8_t value)
{
    static const char digits[] = "0123456789ABCDEF";

    /*
     * Nos quedamos solo con los 4 bits bajos. Asi, 0x0A se convierte en 'A'.
     * Se usa para escribir LL y CC siempre en hexadecimal mayuscula.
     */
    return digits[value & 0x0FU];
}

uint8_t protocol_compute_checksum(const char *data, size_t length)
{
    uint8_t checksum = 0U;
    size_t i;

    if (data == NULL) {
        return 0U;
    }

    /*
     * El checksum XOR arranca en cero y va mezclando cada byte con ^.
     * Ejemplo conceptual: checksum = b0 ^ b1 ^ b2 ^ ... ^ bn.
     * En este protocolo se calcula sobre "LL:TTT:PAYLOAD", sin '@', sin el
     * ultimo ":CC" y sin '\n'.
     */
    for (i = 0U; i < length; i++) {
        checksum ^= (uint8_t) data[i];
    }

    return checksum;
}


// Armamos el frame completo: '@', LL, TTT, PAYLOAD, CC y '\n'.
bool protocol_encode_frame(const protocol_message_t *message, char *frame, size_t frame_size, size_t *frame_length)
{
    const char *type_text;
    size_t payload_length;
    size_t body_length;
    size_t checksum_input_length;
    size_t required_length;
    uint8_t checksum;
    int written;    //Indica el tamaño del buffer que snprintf intentó escribir (sin contar el \0 final)
    char checksum_input[3U + 1U + PROTOCOL_MAX_BODY_SIZE + 1U];

    /*
     * Primero validamos punteros y tamanos. En sistemas embebidos esto evita
     * escribir fuera de un buffer, que es una de las fallas mas dificiles de
     * diagnosticar.
     */
    if ((message == NULL) || (frame == NULL)) {
        return false;
    }

    type_text = protocol_type_to_text(message->type);
    if (type_text[0] == 'I') {
        return false;
    }

    payload_length = strlen(message->payload);
    if ((payload_length > PROTOCOL_MAX_PAYLOAD_LENGTH) ||
        (payload_length != message->payload_length)) {
        return false;
    }

    body_length = PROTOCOL_TYPE_LENGTH + 1U + payload_length;
    if (body_length > PROTOCOL_MAX_BODY_SIZE) {
        return false;
    }

    /*
     * body = "TTT:PAYLOAD".
     * checksum_input = "LL:TTT:PAYLOAD".
     *
     * La longitud LL no cuenta el propio LL ni los separadores externos: solo
     * cuenta el cuerpo TTT:PAYLOAD.
     */
    checksum_input_length = 2U + 1U + body_length;
    if (checksum_input_length >= sizeof(checksum_input)) {
        return false;
    }

    checksum_input[0] = nibble_to_hex((uint8_t) (body_length >> 4));
    checksum_input[1] = nibble_to_hex((uint8_t) body_length);
    checksum_input[2] = PROTOCOL_SEPARATOR_CHAR;

    // En este bloque, written representa el largo de la cadena TTT:PAYLOAD. 
    written = snprintf(&checksum_input[3], sizeof(checksum_input) - 3U,
                       "%s:%s", type_text, message->payload);
    if ((written < 0) || ((size_t) written != body_length)) {
        return false;
    } // confirmamos que el cuerpo se grabó completo en el buffer

    /*
     * Con "LL:TTT:PAYLOAD" ya armado, calculamos CC. Recién al final agregamos
     * '@' adelante, ":CC" y '\n' atras.
     */
    checksum = protocol_compute_checksum(checksum_input, checksum_input_length);
    required_length = 1U + checksum_input_length + 1U + 2U + 1U;
    if (frame_size <= required_length) {
        return false;
    }

    written = snprintf(frame, frame_size, "%c%s:%02X%c",
                       PROTOCOL_START_CHAR,
                       checksum_input,
                       checksum,
                       PROTOCOL_END_CHAR);
    if ((written < 0) || ((size_t) written != required_length)) {
        return false;
    }

    if (frame_length != NULL) {
        *frame_length = required_length;
    }

    return true;
}

bool protocol_validate_frame(const char *frame, protocol_message_t *message)
{
    size_t frame_length;
    size_t body_start;
    size_t checksum_separator;
    size_t expected_frame_length;
    uint8_t expected_body_length;
    uint8_t received_checksum;
    uint8_t computed_checksum;
    int8_t len_hi;
    int8_t len_lo;
    int8_t check_hi;
    int8_t check_lo;

    /*
     * Esta funcion valida una trama completa ya recibida como string:
     * @LL:TTT:PAYLOAD:CC\n
     *
     * Sirve para probar Etapa 1 sin UART. En Etapa 2, el parser incremental
     * hace esta misma validacion pero consumiendo un byte por vez.
     */
    if ((frame == NULL) || (message == NULL)) {
        return false;
    }

    frame_length = strlen(frame);
    if (frame_length < 12U) {
        return false;
    }
    if ((frame[0] != PROTOCOL_START_CHAR) ||
        (frame[3] != PROTOCOL_SEPARATOR_CHAR) ||
        (frame[frame_length - 1U] != PROTOCOL_END_CHAR)) {
        return false;
    }

    len_hi = hex_char_to_nibble(frame[1]);
    len_lo = hex_char_to_nibble(frame[2]);
    if ((len_hi < 0) || (len_lo < 0)) {
        return false;
    }
    expected_body_length = (uint8_t) (((uint8_t) len_hi << 4) | (uint8_t) len_lo);
    if ((expected_body_length < (PROTOCOL_TYPE_LENGTH + 1U)) ||
        (expected_body_length > PROTOCOL_MAX_BODY_SIZE)) {
        return false;
    }

    /*
     * Indices dentro de la trama:
     *   0        -> '@'
     *   1..2     -> LL
     *   3        -> ':'
     *   4..      -> body
     *   despues  -> ':'
     *   despues  -> CC
     *   ultimo   -> '\n'
     */
    body_start = 4U;
    checksum_separator = body_start + expected_body_length;
    expected_frame_length = 1U + 2U + 1U + expected_body_length + 1U + 2U + 1U;
    if (frame_length != expected_frame_length) {
        return false;
    }
    if (frame[checksum_separator] != PROTOCOL_SEPARATOR_CHAR) {
        return false;
    }

    check_hi = hex_char_to_nibble(frame[checksum_separator + 1U]);
    check_lo = hex_char_to_nibble(frame[checksum_separator + 2U]);
    if ((check_hi < 0) || (check_lo < 0)) {
        return false;
    }
    received_checksum = (uint8_t) (((uint8_t) check_hi << 4) | (uint8_t) check_lo);

    /*
     * El checksum se calcula sobre "LL:TTT:PAYLOAD". En la trama completa eso
     * empieza en frame[1] y termina justo antes del separador previo a CC.
     */
    computed_checksum = protocol_compute_checksum(&frame[1], 2U + 1U + expected_body_length);
    if (computed_checksum != received_checksum) {
        return false;
    }

    return protocol_decode_body(&frame[body_start], expected_body_length, message);
}

bool protocol_decode_body(const char *body, uint8_t body_length, protocol_message_t *message)
{
    protocol_type_t type;
    uint8_t payload_length;
    char type_text[PROTOCOL_TYPE_LENGTH + 1U];

    /*
     * Esta funcion recibe solamente el cuerpo ya validado por el parser:
     * "TTT:PAYLOAD". No revisa '@', LL, CC ni '\n'; eso le corresponde a la
     * maquina de estados de parser.c.
     */
    if ((body == NULL) || (message == NULL)) {
        return false;
    }
    if ((body_length < (PROTOCOL_TYPE_LENGTH + 1U)) ||
        (body_length > PROTOCOL_MAX_BODY_SIZE)) {
        return false;
    }
    if (body[PROTOCOL_TYPE_LENGTH] != PROTOCOL_SEPARATOR_CHAR) {
        return false;
    }

    /*
     * Copiamos las tres letras del tipo a un buffer con terminador '\0' para
     * poder tratarlo como string C normal.
     */
    memcpy(type_text, body, PROTOCOL_TYPE_LENGTH);
    type_text[PROTOCOL_TYPE_LENGTH] = '\0';
    type = protocol_type_from_text(type_text);
    if (type == PROTOCOL_TYPE_INVALID) {
        return false;
    }

    payload_length = (uint8_t) (body_length - PROTOCOL_TYPE_LENGTH - 1U);
    if (payload_length > PROTOCOL_MAX_PAYLOAD_LENGTH) {
        return false;
    }

    /*
     * El payload dentro de la trama no viene terminado en '\0'. Por eso se
     * copian exactamente payload_length bytes y despues se agrega el terminador
     * a mano.
     */
    message->type = type;
    message->payload_length = payload_length;
    memcpy(message->payload, &body[PROTOCOL_TYPE_LENGTH + 1U], payload_length);
    message->payload[payload_length] = '\0';

    return true;
}
