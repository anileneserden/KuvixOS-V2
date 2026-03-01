// include/kernel/drivers/input/keyboard.h
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Mod bits: bit0=shift, bit1=ctrl, bit2=alt
uint8_t  kbd_mods(void);

int      kbd_is_ctrl_pressed(void);
int      kbd_is_shift_pressed(void);

int      kbd_is_super_pressed(void);
int      kbd_is_altgr_pressed(void);

void     kbd_init(void);
void     kbd_poll(void);
void     kbd_handler(void);

/*
 * Raw Set1 event:
 *  - Normal tuşlar: 0x00..0xFF (make/break)
 *  - E0 prefix tuşlar: 0xE000 | sc (ör: LeftWin make=0xE05B, break=0xE0DB)
 */
uint16_t kbd_pop_event(void);

int      kbd_has_character(void);

/*
 * Layout + shift ile ASCII üretir.
 * Break event'leri ve E0 event'leri yok sayar.
 */
char     kbd_get_char(void);

// Eski kodlar kırılmasın diye geçici uyumluluk
// (Sadece normal Set1 make scancode için; E0 ile çalışmaz)
char     kbd_scancode_to_ascii(uint8_t sc);

#ifdef __cplusplus
}
#endif