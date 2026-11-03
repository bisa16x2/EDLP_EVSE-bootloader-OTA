#include "ring_buffer.h"

#include <stddef.h>

// inners
static uint16_t ringBufferGetNextPosition(const ring_buffer_t *ring, uint16_t position) {
    position++;

    if (position >= ring->size) position = 0U;

    return position;
}

// functions
bool ringBufferInit(ring_buffer_t *ring, uint8_t *storage, uint16_t size) {
    if (ring == NULL || storage == NULL || size < 2U) return false;

    ring->buffer = storage;
    ring->size = size;
    ring->head = 0U;
    ring->tail = 0U;

    return true;
}

bool ringBufferPut(ring_buffer_t *ring, uint8_t data) {
    if (ring == NULL || ring->buffer == NULL) return false;

    const uint16_t next_head = ringBufferGetNextPosition(ring, ring->head);

    if (next_head == ring->tail) return false;

    ring->buffer[ring->head] = data;
    ring->head = next_head;

    return true;
}

bool ringBufferGet(ring_buffer_t *ring, uint8_t *data) {
    if (ring == NULL || ring->buffer == NULL || data == NULL) return false;
    if (ring->head == ring->tail) return false;

    *data = ring->buffer[ring->tail];

    ring->tail = ringBufferGetNextPosition(ring, ring->tail);

    return true;
}

uint16_t ringBufferAvailable(const ring_buffer_t *ring) {
    if (ring == NULL || ring->buffer == NULL) return 0U;

    const uint16_t head = ring->head;
    const uint16_t tail = ring->tail;

    if (head >= tail) return head - tail;

    return (uint16_t)(ring->size - tail + head);
}

bool ringBufferIsEmpty(const ring_buffer_t *ring) {
    if (ring == NULL) return true;

    return ring->head == ring->tail;
}

bool ringBufferIsFull(const ring_buffer_t *ring) {
    if (ring == NULL || ring->buffer == NULL) return false;

    return ringBufferGetNextPosition(ring, ring->head) == ring->tail;
}

void ringBufferFlush(ring_buffer_t *ring) {
    if (ring == NULL) return;

    ring->tail = ring->head;
}