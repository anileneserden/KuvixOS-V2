#ifndef UI_SETTINGS_H
#define UI_SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

bool ui_get_show_extensions(void);
void ui_set_show_extensions(bool v);
void ui_toggle_show_extensions(void);

uint32_t ui_get_desktop_bg(void);
void     ui_set_desktop_bg(uint32_t argb);

#endif