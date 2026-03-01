#pragma once

#include <app/app.h>
#include <ui/controls/label2.h>

// App state struct (app->user memory boyutu için gerekli)
typedef struct {
    ui_label2_t lbl_title;
    ui_label2_t lbl_1;
    ui_label2_t lbl_2;
    ui_label2_t lbl_3;
    ui_label2_t lbl_4;
    ui_label2_t lbl_5;

    char dollar_hex_str[2];
} demo_font_t;

// VTABLE export
extern const app_vtbl_t demo_font_vtbl;