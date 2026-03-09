#ifndef INPUT_MANAGER_H
#define INPUT_MANAGER_H

#include <stdint.h>

void input_init(void);
void input_update(void);

// Eğer mouse_x/y zaten mouse_ps2.c içinde tanımlıysa burada sadece extern:
extern int mouse_x;
extern int mouse_y;

// Son buton durumu (PS/2 paketinin buttons byte'ı)
uint8_t input_mouse_buttons(void);

// Bu frame biriken dx/dy toplamını verir ve sıfırlar
void input_mouse_frame_delta(int* dx, int* dy);

#endif
