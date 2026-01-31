#pragma once
#include <stdint.h>

void input_init(void);

// Her loop turunda çağır: i8042 (0x64/0x60) boşalana kadar okur,
// byte'ları keyboard/mouse'a dağıtır.
void input_poll(void);

// Klavye event:
// 0 => yok
// (ev & 0xFF00)==0xFF00 => özel tuş, low byte = keycode
// aksi => low byte = karakter
uint16_t input_kbd_pop_event(void);

// Mouse pop:
// 1 => dx/dy/buttons dolu
// 0 => yok
int input_mouse_pop(int* dx, int* dy, uint8_t* buttons);
