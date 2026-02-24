#pragma once
#include <stdint.h>

// session-style desktop API
void ui_desktop_init(void);
void ui_desktop_tick(void);
void ui_desktop_handle_scancode(uint16_t sc);

// UI state klavye ile değişince (hotkey ile pencere açıldı vs.)
// bir sonraki frame full present zorla
void desktop_invalidate_full(void);

// rename callback (desktop_icons edit mode burayı çağırıyor)
void desktop_handle_rename_confirm(const char* new_name);

// legacy (istersen kalsın)
void ui_desktop_run(void);

uint32_t desktop_get_bg_color(void);