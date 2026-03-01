#ifndef KBD_H
#define KBD_H

#include <stdint.h>

typedef struct {
    const char* name;
    const uint8_t* normal;
    const uint8_t* shift;
    const uint8_t* altgr;
} kbd_layout_t;

void kbd_set_layout(const char* name);
const kbd_layout_t* kbd_get_current_layout(void);

#endif