#pragma once
#include <stdint.h>

// session-style desktop API
void ui_desktop_init(void);
void ui_desktop_tick(void);
void ui_desktop_handle_scancode(uint16_t sc);

// rename callback (desktop_icons edit mode burayı çağırıyor)
void desktop_handle_rename_confirm(const char* new_name);

// legacy (istersen kalsın)
void ui_desktop_run(void);