#ifndef MESSAGEBOX_H
#define MESSAGEBOX_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    char title[32];
    char message[64];
    bool visible;
    int x, y, w, h;
} messagebox_t;

// Global erişim için
extern messagebox_t sys_msgbox;

void msgbox_show(const char* title, const char* msg);
void msgbox_draw(void);
bool msgbox_handle_click(int mx, int my);

#endif