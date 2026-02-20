#ifndef PIXEL_DRAW_APP_H
#define PIXEL_DRAW_APP_H

#include <stdint.h>
#include <stdbool.h>

// UI core types (ui_point_t, ui_size_t, ui_rect_t vs.)
#include <ui/controls/ui_context.h>
#include <ui/controls/panel2.h>
#include <ui/controls/button2.h>

typedef struct {
    uint32_t rgba;
    const char* name;
} palette_color_t;

typedef struct {
    int window_id;

    // UI
    ui_context_t ui;
    ui_panel2_t  root;
    ui_panel2_t  toolbar;
    ui_button2_t btn_clear;

    // Palette
    int palette_count;
    palette_color_t palette[8];
    int selected_color;

    // grid (logical canvas)
    int cells_w;
    int cells_h;

    // view
    int cell_px;
    int pixel_pad;
    int pixel_px;

    // canvas location (client-relative)
    int canvas_x;
    int canvas_y;
    int canvas_w_px;
    int canvas_h_px;

    // toolbar height
    int toolbar_h;

    // pixel buffer (RGBA). alpha=0 => transparent
    uint32_t* pixels;

    // hover
    int hover_x;
    int hover_y;
    bool hover_valid;

    bool ctrl_down;

    // current draw color
    uint32_t draw_color;
} pixel_draw_app_t;

#endif