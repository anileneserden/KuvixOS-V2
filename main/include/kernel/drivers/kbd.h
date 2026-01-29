#ifndef KBD_H
#define KBD_H

#include <stdint.h>

void kbd_init(void);
void kbd_poll(void);       // Donanımı kontrol eder (Interrupt gelene kadar)
char kbd_get_char(void);   // Shell'in kullanacağı temiz fonksiyon

#endif