// src/lib/ui/wm/hittest.h
#pragma once

typedef enum {
    HT_NONE = 0,
    HT_CLIENT,
    HT_TITLE,
    HT_BTN_MIN,
    HT_BTN_MAX,
    HT_BTN_CLOSE,
    HT_GRIP_BR
} wm_hittest_t;