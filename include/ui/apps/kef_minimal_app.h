#pragma once

#include <app/app.h>

#define KEF_MAX_WIDGETS 8

typedef struct {
    char type[16];
    char text[128];
    int x;
    int y;
} kef_widget_t;

typedef struct {
    int window_id;
    int loaded;

    char title[128];
    int width;
    int height;

    int widget_count;
    kef_widget_t widgets[KEF_MAX_WIDGETS];
} kef_minimal_state_t;

extern const app_vtbl_t g_kef_minimal_vtbl;