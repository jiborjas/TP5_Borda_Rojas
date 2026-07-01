#include <stdio.h>
#include <string.h>

#include "../../firmware/protocol/parser.h"

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

static parser_result_t feed_bytes(parser_t *parser,
                                  const char *bytes,
                                  protocol_message_t *last_message,
                                  unsigned *ready_count,
                                  unsigned *error_count)
{
    parser_result_t last_result = PARSER_RESULT_IN_PROGRESS;
    size_t i;

    for (i = 0U; bytes[i] != '\0'; i++) {
        last_result = parser_consume_byte(parser, (uint8_t) bytes[i], last_message);
        if (last_result == PARSER_RESULT_MESSAGE_READY) {
            (*ready_count)++;
        } else if (last_result == PARSER_RESULT_ERROR) {
            (*error_count)++;
        }
    }

    return last_result;
}

static void test_valid_ping(void)
{
    parser_t parser;
    protocol_message_t message;
    unsigned ready_count = 0U;
    unsigned error_count = 0U;

    parser_init(&parser);
    expect_true("valid ping ready",
                feed_bytes(&parser, "@08:CMD:ping:52\n", &message,
                           &ready_count, &error_count) == PARSER_RESULT_MESSAGE_READY);
    expect_u8("valid ping ready count", 1U, (uint8_t) ready_count);
    expect_u8("valid ping error count", 0U, (uint8_t) error_count);
    expect_u8("valid ping type", (uint8_t) PROTOCOL_TYPE_CMD, (uint8_t) message.type);
    expect_string("valid ping payload", "ping", message.payload);
}

static void test_noise_before_frame(void)
{
    parser_t parser;
    protocol_message_t message;
    unsigned ready_count = 0U;
    unsigned error_count = 0U;

    parser_init(&parser);
    feed_bytes(&parser, "xyz@08:CMD:ping:52\n", &message, &ready_count, &error_count);
    expect_u8("noise ready count", 1U, (uint8_t) ready_count);
    expect_u8("noise error count", 0U, (uint8_t) error_count);
    expect_string("noise payload", "ping", message.payload);
}

static void test_resync_reuses_start_byte(void)
{
    parser_t parser;
    protocol_message_t message;
    unsigned ready_count = 0U;
    unsigned error_count = 0U;

    parser_init(&parser);
    feed_bytes(&parser, "@0@08:CMD:ping:52\n", &message, &ready_count, &error_count);
    expect_u8("resync ready count", 1U, (uint8_t) ready_count);
    expect_u8("resync error count", 1U, (uint8_t) error_count);
    expect_string("resync payload", "ping", message.payload);
}

static void test_corrupt_checksum(void)
{
    parser_t parser;
    protocol_message_t message;
    unsigned ready_count = 0U;
    unsigned error_count = 0U;

    parser_init(&parser);
    feed_bytes(&parser, "@08:CMD:ping:53\n", &message, &ready_count, &error_count);
    expect_u8("bad checksum ready count", 0U, (uint8_t) ready_count);
    expect_u8("bad checksum error count", 1U, (uint8_t) error_count);
}

static void test_two_frames_back_to_back(void)
{
    parser_t parser;
    protocol_message_t message;
    unsigned ready_count = 0U;
    unsigned error_count = 0U;

    parser_init(&parser);
    feed_bytes(&parser, "@08:CMD:ping:52\n@0A:CMD:led=on:6A\n",
               &message, &ready_count, &error_count);
    expect_u8("two frames ready count", 2U, (uint8_t) ready_count);
    expect_u8("two frames error count", 0U, (uint8_t) error_count);
    expect_string("two frames last payload", "led=on", message.payload);
}

static void test_carriage_return_is_ignored(void)
{
    parser_t parser;
    protocol_message_t message;
    unsigned ready_count = 0U;
    unsigned error_count = 0U;

    parser_init(&parser);
    feed_bytes(&parser, "@08:CMD:ping:52\r\n", &message, &ready_count, &error_count);
    expect_u8("cr ready count", 1U, (uint8_t) ready_count);
    expect_u8("cr error count", 0U, (uint8_t) error_count);
    expect_string("cr payload", "ping", message.payload);
}

int main(void)
{
    test_valid_ping();
    test_noise_before_frame();
    test_resync_reuses_start_byte();
    test_corrupt_checksum();
    test_two_frames_back_to_back();
    test_carriage_return_is_ignored();

    if (g_failures != 0) {
        printf("%d parser test(s) failed\n", g_failures);
        return 1;
    }

    printf("All parser tests passed\n");
    return 0;
}
