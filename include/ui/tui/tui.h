#pragma once
#include <stdint.h>

void tui_init(void);
void tui_tick(void);
void tui_handle_scancode(uint16_t sc);
const char* tui_take_action(void);