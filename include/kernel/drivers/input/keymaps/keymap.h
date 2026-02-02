#pragma once
#include <stdint.h>

typedef struct {
    const uint8_t* normal;   // 128
    const uint8_t* shift;    // 128
} keymap_t;

// şu an sadece TRQ veriyoruz
const keymap_t* keymap_trq(void);
