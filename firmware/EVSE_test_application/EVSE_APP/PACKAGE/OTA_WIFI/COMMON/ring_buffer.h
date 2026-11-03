#ifndef APP_COMMON__RING_BUFFER_H
#define APP_COMMON__RING_BUFFER_H

#include <stdbool.h>
#include <stdint.h>

// tables
typedef struct {
    uint8_t *buffer;
    uint16_t size;

    volatile uint16_t head;
    volatile uint16_t tail;
} ring_buffer_t;

// functions
bool ringBufferInit(ring_buffer_t *ring, uint8_t *storage, uint16_t size);
bool ringBufferPut(ring_buffer_t *ring, uint8_t data);
bool ringBufferGet(ring_buffer_t *ring, uint8_t *data);

uint16_t ringBufferAvailable(const ring_buffer_t *ring);

bool ringBufferIsEmpty(const ring_buffer_t *ring);
bool ringBufferIsFull(const ring_buffer_t *ring);

void ringBufferFlush(ring_buffer_t *ring);

#endif //APP_COMMON__RING_BUFFER_H