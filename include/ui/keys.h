#pragma once
#include <stdint.h>

// i8042 Set1 scancodes (make codes)
// break = make | 0x80

typedef enum {
    KEY_ESC    = 0x01,
    KEY_ENTER  = 0x1C,
    KEY_SPACE  = 0x39,

    KEY_W      = 0x11,
    KEY_S      = 0x1F,

    KEY_UP     = 0x48,
    KEY_DOWN   = 0x50,
} ui_key_t;

static inline int key_is_break(uint8_t sc) { return (sc & 0x80) != 0; }
static inline uint8_t key_make(uint8_t sc) { return (uint8_t)(sc & 0x7F); }