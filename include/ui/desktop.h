#pragma once
#include <stdint.h>

uint32_t desktop_get_bg_color(void);
void     desktop_set_bg_color(uint32_t argb);

// session-style desktop API
void ui_desktop_init(void);
void ui_desktop_tick(void);
void ui_desktop_handle_scancode(uint16_t sc);

void desktop_invalidate_full(void);
void desktop_request_redraw(void);

// rename callback (desktop_icons edit mode burayı çağırıyor)
void desktop_handle_rename_confirm(const char* new_name);

// legacy (istersen kalsın)
void ui_desktop_run(void);