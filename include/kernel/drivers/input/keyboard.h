#pragma once
#include <stdint.h>

// Mod bits: bit0=shift, bit1=ctrl, bit2=alt
uint8_t  kbd_mods(void);

// Basılı tutulma durumunu sorgulamak için yeni bir fonksiyon prototipi
int kbd_is_ctrl_pressed(void);
int kbd_is_shift_pressed(void);
void     kbd_init(void);
void     kbd_poll(void);
void     kbd_handler(void);

uint16_t kbd_pop_event(void);   // raw Set1 scancode (make/break)
int      kbd_has_character(void);
char     kbd_get_char(void);    // layout + shift ile ASCII üretir (break'leri yok sayar)

// (opsiyonel) layout seçmek istersen
void     kbd_set_layout_us(void);
void     kbd_set_layout_trq(void);

// Eski kodlar kırılmasın diye geçici uyumluluk
char kbd_scancode_to_ascii(uint8_t sc);