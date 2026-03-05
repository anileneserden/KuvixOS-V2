#ifndef NOTIFICATION_H
#define NOTIFICATION_H

#include <stdint.h>
#include <stdbool.h>

void notification_show(const char* text, uint32_t duration);
void notification_draw(void);

void notification_tick(int delta_ms);
int  notification_is_visible(void);

#endif