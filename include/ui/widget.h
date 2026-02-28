#pragma once
#include <stdint.h>

typedef enum {
    WIDGET_LABEL  = 1,
    WIDGET_BUTTON = 2
} widget_type_t;

typedef struct widget {
    widget_type_t type;

    int x, y;
    int w, h;

    char text[64];

    void (*on_click)(void* user);
    void* user;

    int visible;
} widget_t;