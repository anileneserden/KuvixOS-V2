#ifndef KBD_H
#define KBD_H

#include <stdint.h>

// Klavye düzeni yapısı
typedef struct {
    const char* name;
    const uint8_t* normal;
    const uint8_t* shift;
} kbd_layout_t;

void kbd_init(void);
void kbd_poll(void);
char kbd_get_char(void);
void kbd_handle_byte(uint8_t sc);

// Layout yönetimi fonksiyonları
void kbd_set_layout(const char* name);
const kbd_layout_t* kbd_get_current_layout(void);

#endif