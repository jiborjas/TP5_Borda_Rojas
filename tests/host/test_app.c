#include <stdio.h>
#include <string.h>

#include "stubs/FreeRTOS.h"
#include "../../firmware/app/app.h"

typedef struct {
    protocol_message_t tx_message;
    actuator_command_t actuator_command;
    unsigned tx_count;
    unsigned actuator_count;
} fake_queues_t;

static int g_failures = 0;
static fake_queues_t g_queues;

static int g_tx_queue_token;
static int g_actuator_queue_token;
static QueueHandle_t g_tx_queue = &g_tx_queue_token;
static QueueHandle_t g_actuator_queue = &g_actuator_queue_token;

BaseType_t xQueueSend(QueueHandle_t queue, const void *item, uint32_t ticks_to_wait)
{
    (void) ticks_to_wait;

    if (queue == g_tx_queue) {
        memcpy(&g_queues.tx_message, item, sizeof(g_queues.tx_message));
        g_queues.tx_count++;
        return pdPASS;
    }
    if (queue == g_actuator_queue) {
        memcpy(&g_queues.actuator_command, item, sizeof(g_queues.actuator_command));
        g_queues.actuator_count++;
        return pdPASS;
    }

    return 0;
}

uint32_t uart_comm_get_rx_irq_count(void)
{
    return 7U;
}

uint32_t uart_comm_get_rx_drop_count(void)
{
    return 1U;
}

uint32_t tasks_get_parser_byte_count(void)
{
    return 11U;
}

uint32_t tasks_get_parser_message_count(void)
{
    return 5U;
}

uint32_t tasks_get_parser_error_count(void)
{
    return 2U;
}

bool sensors_build_telemetry_payload(char *payload, size_t payload_size, uint32_t sequence)
{
    (void) sequence;

    if (payload_size < 4U) {
        return false;
    }

    memcpy(payload, "t=0", 4U);
    return true;
}

static void reset_fake_queues(void)
{
    memset(&g_queues, 0, sizeof(g_queues));
}

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

static void send_message(protocol_type_t type, const char *payload)
{
    protocol_message_t message;

    expect_true("set input message", protocol_message_set(&message, type, payload));
    app_handle_message(&message, g_tx_queue, g_actuator_queue);
}

static void test_ping(void)
{
    reset_fake_queues();
    app_init();

    send_message(PROTOCOL_TYPE_CMD, "ping");
    expect_u8("ping tx count", 1U, (uint8_t) g_queues.tx_count);
    expect_u8("ping response type", (uint8_t) PROTOCOL_TYPE_ACK, (uint8_t) g_queues.tx_message.type);
    expect_string("ping response payload", "pong=1", g_queues.tx_message.payload);
}

static void test_led_on(void)
{
    reset_fake_queues();
    app_init();

    send_message(PROTOCOL_TYPE_CMD, "led=on");
    expect_u8("led tx count", 1U, (uint8_t) g_queues.tx_count);
    expect_u8("led actuator count", 1U, (uint8_t) g_queues.actuator_count);
    expect_u8("led response type", (uint8_t) PROTOCOL_TYPE_ACK, (uint8_t) g_queues.tx_message.type);
    expect_string("led response payload", "cmd=ok", g_queues.tx_message.payload);
    expect_string("led target", "led", g_queues.actuator_command.target);
    expect_string("led action", "on", g_queues.actuator_command.action);
}

static void test_errors(void)
{
    reset_fake_queues();
    app_init();

    send_message(PROTOCOL_TYPE_CMD, "no_existe");
    expect_u8("unknown response type", (uint8_t) PROTOCOL_TYPE_ERR, (uint8_t) g_queues.tx_message.type);
    expect_string("unknown response payload", "code=unknown_cmd", g_queues.tx_message.payload);

    send_message(PROTOCOL_TYPE_ACK, "cmd=ok");
    expect_u8("unexpected response type", (uint8_t) PROTOCOL_TYPE_ERR, (uint8_t) g_queues.tx_message.type);
    expect_string("unexpected response payload", "code=unexpected_type", g_queues.tx_message.payload);
}

static void test_status(void)
{
    reset_fake_queues();
    app_init();

    send_message(PROTOCOL_TYPE_CMD, "ping");
    send_message(PROTOCOL_TYPE_CMD, "no_existe");
    send_message(PROTOCOL_TYPE_CMD, "status?");

    expect_u8("status response type", (uint8_t) PROTOCOL_TYPE_STS, (uint8_t) g_queues.tx_message.type);
    expect_string("status payload",
                  "rx=3,ae=1,irq=7,pb=11,pm=5,pe=2,qd=1",
                  g_queues.tx_message.payload);
}

int main(void)
{
    test_ping();
    test_led_on();
    test_errors();
    test_status();

    if (g_failures != 0) {
        printf("%d app test(s) failed\n", g_failures);
        return 1;
    }

    printf("All app tests passed\n");
    return 0;
}
