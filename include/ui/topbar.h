#ifndef TOPBAR_H
#define TOPBAR_H

#include <stdint.h>

void topbar_init(void);
void topbar_draw(void);
// İleride saat veya menü tıklaması için:
void topbar_handle_mouse(int mx, int my, int pressed);

#endif