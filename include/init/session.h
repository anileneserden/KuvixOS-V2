#ifndef SESSION_H
#define SESSION_H

#include <stdint.h>

void session_init(void);
void session_handle_scancode(uint16_t scancode);
void session_tick(void); // Yeni eklenen frame/render kontrolü

#endif