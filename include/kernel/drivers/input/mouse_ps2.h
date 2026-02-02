#ifndef MOUSE_PS2_H
#define MOUSE_PS2_H

#include <stdint.h>

extern int mouse_x;
extern int mouse_y;

void ps2_mouse_init(void);
void mouse_handler(void);
void ps2_mouse_handle_byte(uint8_t data);
int ps2_mouse_pop(int* dx, int* dy, uint8_t* buttons);
void ps2_mouse_poll(void);
void ps2_mouse_update(void);

#endif