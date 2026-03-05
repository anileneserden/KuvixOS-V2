#ifndef UI_TUI_H
#define UI_TUI_H

#include <stdint.h>

void tui_clear(void);

void tui_set_title(const char* t);
void tui_add_item(const char* label, const char* action);

void tui_init(void);
void tui_tick(void);
void tui_handle_scancode(uint16_t sc);

int  tui_get_item_count(void);
int  tui_get_selected(void);
void tui_set_selected(int idx);
void tui_notify(const char* msg, int ms);

#endif