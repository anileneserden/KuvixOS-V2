#pragma once
#include <stdint.h>

#define MOUSE_BTN_L 0x01
#define MOUSE_BTN_R 0x02
#define MOUSE_BTN_M 0x04

void ps2_mouse_init(void);

// input core buraya mouse byte’ı gönderir
void ps2_mouse_handle_byte(uint8_t data);

// consumer buradan çeker
int  ps2_mouse_pop(int* dx, int* dy, uint8_t* buttons);

// Eğer header'da yoksa buraya ekle:
void ps2_mouse_poll(void);