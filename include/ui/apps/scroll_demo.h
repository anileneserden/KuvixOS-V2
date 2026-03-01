#pragma once
#include <app/app.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    // viewport (client coords) — koyu gri alan
    int vx, vy, vw, vh;

    // content
    int content_h;    // içerik toplam yüksekliği (px)
    int offset_y;     // 0..max
    int wheel_px;     // 1 wheel step kaç px kaydırsın

    // mouse (client coords) — hover için
    int mx, my;
} scroll_demo_t;

extern const app_vtbl_t scroll_demo_vtbl;

#ifdef __cplusplus
}
#endif