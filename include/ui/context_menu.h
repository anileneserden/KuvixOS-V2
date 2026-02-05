#ifndef CONTEXT_MENU_H
#define CONTEXT_MENU_H

#include <stdbool.h>

void context_menu_reset(void);
void context_menu_add_item(const char* text, void (*callback)(void));
void context_menu_show(int x, int y);
void context_menu_hide(void);
bool context_menu_is_visible(void);
void context_menu_draw(void);
void context_menu_handle_mouse(int mx, int my, bool clicked);

#endif