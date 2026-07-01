#ifndef TEST_STUB_QUEUE_H
#define TEST_STUB_QUEUE_H

#include <stdint.h>

typedef void *QueueHandle_t;
typedef int BaseType_t;

BaseType_t xQueueSend(QueueHandle_t queue, const void *item, uint32_t ticks_to_wait);

#endif
