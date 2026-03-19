#pragma once

#include <app/app.h>
#include <stdint.h>

#define KEF_MAX_WIDGETS 16

#define KEF_WIDGET_LABEL  1
#define KEF_WIDGET_BUTTON 2

typedef struct kef_minimal_state kef_minimal_state_t;
typedef struct kef_widget kef_widget_t;

struct kef_widget {
    char id[32];
    int type;

    int x;
    int y;
    int w;
    int h;

    char text[128];
    uint32_t text_color;

    int pressed;
    int hovered;

    kef_minimal_state_t* owner;

    void (*on_click)(kef_widget_t* self);
};

struct kef_minimal_state {
    int window_id;
    int loaded;

    char title[128];
    int width;
    int height;
    uint32_t bg_color;

    int widget_count;
    kef_widget_t widgets[KEF_MAX_WIDGETS];
};

extern const app_vtbl_t g_kef_minimal_vtbl;

kef_widget_t* kef_get_widget_ptr(kef_minimal_state_t* st, const char* id);
void kef_set_text(kef_minimal_state_t* st, const char* id, const char* text);