// include/ui/widget.h
#pragma once
#include <stdint.h>

typedef enum {
    WIDGET_NONE = 0,
    WIDGET_LABEL,
    WIDGET_BUTTON,
} widget_type_t;

typedef struct widget {
    widget_type_t type;
    int x, y, w, h;
    int visible;

    // state
    int hovered;
    int pressed;

    char text[64];

    void (*on_click)(void* user);
    void* user;
} widget_t;