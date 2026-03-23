#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    KEF_WIDGET_LABEL = 1,
    KEF_WIDGET_BUTTON = 2,
    KEF_WIDGET_INPUT = 3
} kef_widget_type_t;

typedef struct kef_widget {
    int type;

    char id[64];
    char text[256];

    int x;
    int y;
    int w;
    int h;

    uint32_t text_color;

    char value[256];
    int value_len;
    int focused;

    void* owner;
    void (*on_click)(struct kef_widget* self);
} kef_widget_t;

typedef struct kef_minimal_state {
    char title[128];
    int width;
    int height;
    uint32_t bg_color;

    int loaded;
    int widget_count;

    kef_widget_t widgets[64];
} kef_minimal_state_t;

kef_widget_t* kef_get_widget_ptr(kef_minimal_state_t* st, const char* id);
void kef_set_text(kef_minimal_state_t* st, const char* id, const char* text);

#ifdef __cplusplus
}
#endif