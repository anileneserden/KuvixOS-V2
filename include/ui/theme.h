// include/ui/theme.h
#pragma once
#include <stdint.h>
#include <kernel/drivers/video/fb.h>  // Yolu güncelledik (fb_color_t buradan gelir)

typedef enum { UI_ALIGN_LEFT=0, UI_ALIGN_CENTER=1, UI_ALIGN_RIGHT=2 } ui_align_t;
typedef enum { UI_BTN_LAYOUT_LEFT=0, UI_BTN_LAYOUT_RIGHT=1 } ui_btn_layout_t;
typedef enum { UI_BTN_STYLE_CLASSIC=0, UI_BTN_STYLE_TRAFFIC=1 } ui_btn_style_t;
typedef enum { UI_CAPBTN_STYLE_TRAFFIC = 0, UI_CAPBTN_STYLE_WINDOWS = 1, UI_CAPBTN_STYLE_FLAT = 2 } ui_capbtn_style_t;

typedef struct {
    fb_color_t desktop_bg;
    fb_color_t window_bg;
    fb_color_t window_border;
    fb_color_t window_title_bg;
    fb_color_t window_title_text;
    int window_corner_radius;

    int window_border_px;
    int window_title_h;
    int window_radius_px;
    int window_clip_client;
    ui_align_t window_title_align;
    int window_title_pad_l;
    int window_title_pad_r;

    ui_btn_style_t  window_btn_style;
    ui_btn_layout_t window_btn_layout;
    int window_btn_size;
    int window_btn_gap;
    int window_btn_margin;
    int window_btn_pad_left;
    int window_btn_pad_right;

    uint8_t window_btn_order[3];
    fb_color_t window_btn_close;
    fb_color_t window_btn_max;
    fb_color_t window_btn_min;

    int window_btn_hover_dark;
    int window_btn_press_dark;

    // ---- Caption buttons (titlebar buttons) ----
    ui_capbtn_style_t capbtn_style;

    int capbtn_icon_enabled;   // 0/1
    int capbtn_icon_size;      // 16 default
    int capbtn_radius;         // 0 kare, büyük sayı = yuvarlak
    int capbtn_outline_px;     // 0/1

    // Traffic style renkleri
    fb_color_t capbtn_bg_close;
    fb_color_t capbtn_bg_max;
    fb_color_t capbtn_bg_min;

    // Windows/Flat style hover/press bg
    fb_color_t capbtn_bg_hover;
    fb_color_t capbtn_bg_press;

    // ikon rengi
    fb_color_t capbtn_icon;
    fb_color_t capbtn_icon_hover;
    fb_color_t capbtn_icon_press;

    int capbtn_close_red_on_hover;

    fb_color_t textbox_bg;
    fb_color_t textbox_border;
    fb_color_t textbox_focus_border;
    fb_color_t textbox_text;
    fb_color_t textbox_placeholder;
    fb_color_t textbox_caret;

    fb_color_t button_bg;
    fb_color_t button_border;
    fb_color_t button_hover_bg;
    fb_color_t button_hover_border;
    fb_color_t button_pressed_bg;
    fb_color_t button_pressed_border;
    fb_color_t button_text;

    fb_color_t select_bg;
    fb_color_t select_border;

    fb_color_t dock_bg;
    fb_color_t dock_border;
    fb_color_t dock_icon_bg;
    int dock_height;
    int dock_radius;
    int dock_margin_bottom;
    int dock_padding_x;
    int dock_gap;
    int dock_icon_size;
    int dock_icon_hover_bg;
    int dock_spacing;
    int dock_position;
    int dock_auto_hide;
} ui_theme_t;

const ui_theme_t* ui_get_theme(void);
void ui_set_theme(const ui_theme_t* th);
const ui_theme_t* ui_get_builtin_theme(void);
void ui_theme_load_from_kth(const char* text, ui_theme_t* out);
void ui_theme_bootstrap_default(void);