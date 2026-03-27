#pragma once
#include <app/app.h>

#include <ui/controls/button2.h>
#include <ui/controls/label2.h>
#include <ui/controls/combobox2.h>
#include <ui/controls/textbox2.h>

typedef struct {
    // controls
    ui_label2_t  title;
    ui_label2_t  status;

    ui_button2_t btn_inc;
    ui_combobox2_t  combo;

    textbox2_t    tb;

    // model
    int counter;

    // focus routing: şimdilik sadece textbox focus’lu olunca key ona gider
} controls_test_t;

extern const app_vtbl_t controls_test_vtbl;