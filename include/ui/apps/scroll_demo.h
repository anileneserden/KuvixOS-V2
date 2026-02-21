#pragma once
#include <app/app.h>   // app_vtbl_t burada tanımlı olmalı

#ifdef __cplusplus
extern "C" {
#endif

#define SCROLL_DEMO_MAX_LINES  2048
#define SCROLL_DEMO_LINE_MAX   128

typedef struct {
    char lines[SCROLL_DEMO_MAX_LINES][SCROLL_DEMO_LINE_MAX];
    int line_count;
    int view_start;

    int line_h;
    int cols;
    int rows;

    int sb_dragging;
    int sb_track_x, sb_track_y, sb_track_w, sb_track_h;
    int sb_thumb_y, sb_thumb_h;
    int sb_grab_off; // thumb içinden tuttuğun offset (my - thumb_y)
    int sb_drag_off_y;
    int sb_drag_off_x;    
} scroll_demo_t;

// ✅ Doğru olan: typedef'i kullan
extern const app_vtbl_t scroll_demo_vtbl;

#ifdef __cplusplus
}
#endif