// src/lib/ui/wm/hittest.h
#pragma once

typedef enum {
    HT_NONE = 0,
    HT_TITLE,
    HT_BTN_CLOSE,
    HT_BTN_MIN,
    HT_BTN_MAX,
    HT_CLIENT,
    HT_GRIP_BR, // Bottom-right grip

    // Boyutlandırma Kenarları
    HT_RESIZE_LEFT,
    HT_RESIZE_RIGHT,
    HT_RESIZE_TOP,
    HT_RESIZE_BOTTOM,
    HT_RESIZE_TOP_LEFT,
    HT_RESIZE_TOP_RIGHT,
    HT_RESIZE_BOTTOM_LEFT,
    HT_RESIZE_BOTTOM_RIGHT
} wm_hittest_t;

// Hit-test sonuçlarına ekle
#define HT_RESIZE_RIGHT_BOTTOM 10