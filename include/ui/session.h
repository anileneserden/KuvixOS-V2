#pragma once
#include <stdint.h>

typedef enum {
    UI_SESSION_NONE = 0,
    UI_SESSION_TTY1 = 1,
    UI_SESSION_DESKTOP = 2,
} ui_session_t;

void ui_session_init(void);
void ui_session_switch(ui_session_t s);
void ui_session_tick(void);

// 🔥 kmain loop'tan klavyeyi session'a iletmek için
void ui_session_handle_scancode(uint16_t sc);