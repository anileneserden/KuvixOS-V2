#pragma once
#include <stdint.h>

typedef struct app app_t;

typedef struct app_vtbl {
    void (*on_create)(app_t* a);
    void (*on_destroy)(app_t* a);
    void (*on_mouse)(app_t* a, int mx, int my, uint8_t pr, uint8_t rel, uint8_t btn);
    void (*on_key)(app_t* a, uint16_t keyev);
    void (*on_update)(app_t* a);
    void (*on_draw)(app_t* a);
    int (*on_close_request)(app_t* self);
    void (*on_wheel)(app_t* app, int wheel);
    // --- Titlebar Tabs (optional) ---
    int  (*tabs_count)(struct app* app);
    const char* (*tabs_title)(struct app* app, int idx);
    int  (*tabs_active)(struct app* app);
    void (*tabs_set_active)(struct app* app, int idx);
} app_vtbl_t;

struct app {
    int win_id;
    void* user;
    const app_vtbl_t* v;
    int visible;
    int id;

    // ✅ Oyun / animasyon / designer gibi app'ler için:
    // 1 => her frame redraw/present zorla
    int wants_continuous_redraw;
};