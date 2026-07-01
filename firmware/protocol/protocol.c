#include <string.h>

#include "protocol.h"

/*
 * Este archivo fue dejado intencionalmente incompleto para la práctica.
 *
 * Objetivo:
 * - construir tramas del tipo @LL:TTT:PAYLOAD:CC\n
 * - calcular checksum XOR de LL:TTT:PAYLOAD
 * - decodificar el body TTT:PAYLOAD
 *
 * Documentación útil:
 * - docs/PROTOCOL.md
 * - docs/FRAME_GENERATION_AND_PARSING.md
 */

const char *protocol_type_to_text(protocol_type_t type)
{
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

bool protocol_message_set(protocol_message_t *message, protocol_type_t type, const char *payload)
{
    size_t payload_length;

    if ((message == NULL) || (payload == NULL)) {
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

uint8_t protocol_compute_checksum(const char *data, size_t length)
{
    (void) data;
    (void) length;

    /* TODO(alumno): implementar XOR byte a byte. */
    return 0U;
}

bool protocol_encode_frame(const protocol_message_t *message, char *frame, size_t frame_size, size_t *frame_length)
{
    (void) message;
    (void) frame;
    (void) frame_size;
    (void) frame_length;

    /*
     * TODO(alumno):
     * 1. construir body = "TTT:PAYLOAD"
     * 2. calcular LL en hexadecimal
     * 3. calcular checksum sobre "LL:TTT:PAYLOAD"
     * 4. armar la trama final @LL:TTT:PAYLOAD:CC\n
     */
    return false;
}

bool protocol_decode_body(const char *body, uint8_t body_length, protocol_message_t *message)
{
    (void) body;
    (void) body_length;
    (void) message;

    /*
     * TODO(alumno):
     * - validar formato TTT:PAYLOAD
     * - convertir TTT a protocol_type_t
     * - copiar payload al struct message
     */
    return false;
}
