#pragma once
#include <stdint.h>

// Özel event formatı: 0xFF00 | KEY
enum {
    KBD_NONE = 0,
    KBD_F1 = 0x01,
    KBD_F2 = 0x02,
    KBD_F3 = 0x03,
    KBD_F4 = 0x04,
    KBD_F5 = 0x05,
    KBD_F6 = 0x06,
};

void     kbd_init(void);
void     kbd_set_layout_trq(void);   // şimdilik TRQ
void     kbd_handle_byte(uint8_t sc);
uint16_t kbd_pop_event(void);
int      kbd_get_key(void);          // 0 yoksa, aksi halde char veya 0xFF00|KBD_Fn
void     kbd_debug_set(int enabled);
int      kbd_debug_get(void);

// Debug (istersen kullan)
extern volatile uint8_t g_kbd_last_sc;
extern volatile uint8_t g_kbd_last_is_break;
extern volatile uint8_t g_kbd_last_e0;
