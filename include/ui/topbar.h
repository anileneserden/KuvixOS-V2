#ifndef TOPBAR_H
#define TOPBAR_H

#include <stdint.h>

void topbar_init(void);
void topbar_draw(void);
void topbar_handle_mouse(int mx, int my);
void topbar_tick();
int topbar_consume_dirty(void);

#endif