#include <stdio.h>
#include <string.h>

#include "../../firmware/protocol/protocol.h"

static int g_failures = 0;

static void expect_true(const char *name, bool condition)
{
    if (!condition) {
        printf("FAIL: %s\n", name);
        g_failures++;
    }
}

static void expect_u8(const char *name, uint8_t expected, uint8_t actual)
{
    if (expected != actual) {
        printf("FAIL: %s expected=%u actual=%u\n", name, expected, actual);
        g_failures++;
    }
}

static void expect_string(const char *name, const char *expected, const char *actual)
{
    if (strcmp(expected, actual) != 0) {
        printf("FAIL: %s expected=[%s] actual=[%s]\n", name, expected, actual);
        g_failures++;
    }
}

static void test_checksum_examples(void)
{
    expect_u8("checksum ping", 0x52U,
              protocol_compute_checksum("08:CMD:ping", strlen("08:CMD:ping")));
    expect_u8("checksum led=on", 0x6AU,
              protocol_compute_checksum("0A:CMD:led=on", strlen("0A:CMD:led=on")));
    expect_u8("checksum led=toggle", 0x7DU,
              protocol_compute_checksum("0E:CMD:led=toggle", strlen("0E:CMD:led=toggle")));
    expect_u8("checksum ACK cmd=ok", 0x6BU,
              protocol_compute_checksum("0A:ACK:cmd=ok", strlen("0A:ACK:cmd=ok")));
}

static void test_encode_examples(void)
{
    protocol_message_t message;
    char frame[PROTOCOL_MAX_FRAME_SIZE];
    size_t frame_length = 0U;

    expect_true("set CMD ping", protocol_message_set(&message, PROTOCOL_TYPE_CMD, "ping"));
    expect_true("encode CMD ping", protocol_encode_frame(&message, frame, sizeof(frame), &frame_length));
    expect_string("frame CMD ping", "@08:CMD:ping:52\n", frame);
    expect_u8("frame length CMD ping", (uint8_t) strlen("@08:CMD:ping:52\n"), (uint8_t) frame_length);

    expect_true("set CMD led=on", protocol_message_set(&message, PROTOCOL_TYPE_CMD, "led=on"));
    expect_true("encode CMD led=on", protocol_encode_frame(&message, frame, sizeof(frame), &frame_length));
    expect_string("frame CMD led=on", "@0A:CMD:led=on:6A\n", frame);

    expect_true("set CMD led=off", protocol_message_set(&message, PROTOCOL_TYPE_CMD, "led=off"));
    expect_true("encode CMD led=off", protocol_encode_frame(&message, frame, sizeof(frame), &frame_length));
    expect_string("frame CMD led=off", "@0B:CMD:led=off:07\n", frame);

    expect_true("set CMD led=toggle", protocol_message_set(&message, PROTOCOL_TYPE_CMD, "led=toggle"));
    expect_true("encode CMD led=toggle", protocol_encode_frame(&message, frame, sizeof(frame), &frame_length));
    expect_string("frame CMD led=toggle", "@0E:CMD:led=toggle:7D\n", frame);

    expect_true("set CMD status?", protocol_message_set(&message, PROTOCOL_TYPE_CMD, "status?"));
    expect_true("encode CMD status?", protocol_encode_frame(&message, frame, sizeof(frame), &frame_length));
    expect_string("frame CMD status?", "@0B:CMD:status?:13\n", frame);

    expect_true("set ACK pong=1", protocol_message_set(&message, PROTOCOL_TYPE_ACK, "pong=1"));
    expect_true("encode ACK pong=1", protocol_encode_frame(&message, frame, sizeof(frame), &frame_length));
    expect_string("frame ACK pong=1", "@0A:ACK:pong=1:22\n", frame);

    expect_true("set ACK cmd=ok", protocol_message_set(&message, PROTOCOL_TYPE_ACK, "cmd=ok"));
    expect_true("encode ACK cmd=ok", protocol_encode_frame(&message, frame, sizeof(frame), &frame_length));
    expect_string("frame ACK cmd=ok", "@0A:ACK:cmd=ok:6B\n", frame);

    expect_true("set ERR unknown", protocol_message_set(&message, PROTOCOL_TYPE_ERR, "code=unknown_cmd"));
    expect_true("encode ERR unknown", protocol_encode_frame(&message, frame, sizeof(frame), &frame_length));
    expect_string("frame ERR unknown", "@14:ERR:code=unknown_cmd:2D\n", frame);
}

static void test_decode_body(void)
{
    protocol_message_t message;

    expect_true("decode CMD ping", protocol_decode_body("CMD:ping", 8U, &message));
    expect_u8("decode type CMD", (uint8_t) PROTOCOL_TYPE_CMD, (uint8_t) message.type);
    expect_string("decode payload ping", "ping", message.payload);
    expect_u8("decode payload length ping", 4U, message.payload_length);

    expect_true("reject invalid separator", !protocol_decode_body("CMD-ping", 8U, &message));
    expect_true("reject invalid type", !protocol_decode_body("BAD:ping", 8U, &message));
}

static void test_validate_frame(void)
{
    protocol_message_t message;

    expect_true("validate valid ping frame",
                protocol_validate_frame("@08:CMD:ping:52\n", &message));
    expect_u8("validate type CMD", (uint8_t) PROTOCOL_TYPE_CMD, (uint8_t) message.type);
    expect_string("validate payload ping", "ping", message.payload);

    expect_true("reject corrupt checksum",
                !protocol_validate_frame("@08:CMD:ping:53\n", &message));
    expect_true("reject wrong length",
                !protocol_validate_frame("@09:CMD:ping:52\n", &message));
    expect_true("reject missing end",
                !protocol_validate_frame("@08:CMD:ping:52", &message));
}

static void test_hex_char_to_nibble(void)
{
    expect_u8("hex 0", 0U, (uint8_t) hex_char_to_nibble('0'));
    expect_u8("hex 9", 9U, (uint8_t) hex_char_to_nibble('9'));
    expect_u8("hex A", 10U, (uint8_t) hex_char_to_nibble('A'));
    expect_u8("hex f", 15U, (uint8_t) hex_char_to_nibble('f'));
    expect_true("hex invalid", hex_char_to_nibble('x') < 0);
}

int main(void)
{
    test_checksum_examples();
    test_encode_examples();
    test_decode_body();
    test_validate_frame();
    test_hex_char_to_nibble();

    if (g_failures != 0) {
        printf("%d test(s) failed\n", g_failures);
        return 1;
    }

    printf("All protocol tests passed\n");
    return 0;
}
