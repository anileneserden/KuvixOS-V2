#ifndef KBD_H
#define KBD_H

#include <stdint.h>

// Sürücüyü ilklendirir
void kbd_init(void);

// Donanımı sorgular (Polling)
void kbd_poll(void);

// Kuyruktan bir karakter çeker (Yoksa 0 döner)
char kbd_get_char(void);

// Scancode işleme (Dahili kullanım için prototip)
void kbd_handle_byte(uint8_t sc);

#endif