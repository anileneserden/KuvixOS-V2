#pragma once
#include <stdint.h>

enum {
    KBD_NONE = 0,

    KBD_F1 = 0x01,
    KBD_F2 = 0x02,
    KBD_F3 = 0x03,
    KBD_F4 = 0x04,
    KBD_F5 = 0x05,
    KBD_F6 = 0x06,

    // 🔥 YENİ
    KBD_UP    = 0x10,
    KBD_DOWN  = 0x11,
    KBD_LEFT  = 0x12,
    KBD_RIGHT = 0x13,
};

void     kbd_init(void);
void     kbd_set_layout_trq(void);
void     kbd_handle_byte(uint8_t sc);
uint16_t kbd_pop_event(void);

int      kbd_get_char(void);   // 🔥 artık int

int      kbd_has_character(void);
void     kbd_debug_set(int enabled);
int      kbd_debug_get(void);

extern volatile uint8_t g_kbd_last_sc;
extern volatile uint8_t g_kbd_last_is_break;
extern volatile uint8_t g_kbd_last_e0;