// include/ui/apps/settings/settings_app.h
#ifndef SETTINGS_APP_H
#define SETTINGS_APP_H

#include <stdint.h>

void settings_init(void);
void settings_update(int mx, int my, uint8_t b);
void settings_draw(int mx, int my);

#endif