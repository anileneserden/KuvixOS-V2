// ui/wm/hittest.h
#pragma once

typedef enum {
    HT_NONE = 0,
    HT_TITLE,
    HT_BTN_CLOSE,
    HT_BTN_MIN,
    HT_BTN_MAX,
    HT_CLIENT,
    HT_GRIP_BR,
    HT_RESIZE_LEFT,
    HT_RESIZE_RIGHT,
    HT_RESIZE_TOP,
    HT_RESIZE_BOTTOM,
    HT_RESIZE_TOP_LEFT,
    HT_RESIZE_TOP_RIGHT,
    HT_RESIZE_BOTTOM_LEFT,
    HT_RESIZE_BOTTOM_RIGHT,
    HT_RESIZE_RIGHT_BOTTOM,

    // Titlebar tab hitleri (şimdilik wm.c'de kullanıyoruz)
    HT_TAB0 = 100,
    HT_TAB1 = 101,
    HT_TAB2 = 102,
    HT_TAB_ADD = 110
} wm_hittest_t;