// include/kernel/drivers/input/keyboard.h
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    KBD_NONE = 0,

    KBD_F1 = 0x01,
    KBD_F2 = 0x02,
    KBD_F3 = 0x03,
    KBD_F4 = 0x04,
    KBD_F5 = 0x05,
    KBD_F6 = 0x06,

    KBD_UP    = 0x10,
    KBD_DOWN  = 0x11,
    KBD_LEFT  = 0x12,
    KBD_RIGHT = 0x13,
};

/*
 * Modifier bits:
 * bit0 = shift
 * bit1 = ctrl
 * bit2 = alt
 */
uint8_t  kbd_mods(void);

int      kbd_is_ctrl_pressed(void);
int      kbd_is_shift_pressed(void);
int      kbd_is_super_pressed(void);
int      kbd_is_altgr_pressed(void);

void     kbd_init(void);
void     kbd_poll(void);
void     kbd_handler(void);

void     kbd_set_layout_trq(void);
void     kbd_handle_byte(uint8_t sc);

/*
 * Raw keyboard event queue:
 *  - normal scancode: 0x00..0xFF
 *  - special decoded keys via higher-level handling as needed
 */
uint16_t kbd_pop_event(void);

int      kbd_has_character(void);

/*
 * Returns:
 *  - 0 if no key
 *  - ASCII char for normal printable/input keys
 *  - 0xFF00 | KBD_* for special keys like arrows
 */
int      kbd_get_char(void);

/*
 * Legacy compatibility helper.
 * Only for plain non-E0 scancode to ASCII conversion.
 */
char     kbd_scancode_to_ascii(uint8_t sc);

void     kbd_debug_set(int enabled);
int      kbd_debug_get(void);

extern volatile uint8_t g_kbd_last_sc;
extern volatile uint8_t g_kbd_last_is_break;
extern volatile uint8_t g_kbd_last_e0;

#ifdef __cplusplus
}
#endif