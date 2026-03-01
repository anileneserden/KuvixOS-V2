#pragma once
#include <ui/controls/control.h>

typedef void (*ui_click2_fn)(void* user);

typedef struct {
    ui_control_t base;
    const char* label; // UTF-8

    ui_click2_fn on_click;
    void* on_click_user;
} ui_button2_t;

void ui_button2_init(ui_button2_t* b, int id, ui_point_t loc, ui_size_t size, const char* label);
void ui_button2_onclick(ui_button2_t* b, ui_click2_fn fn, void* user);
