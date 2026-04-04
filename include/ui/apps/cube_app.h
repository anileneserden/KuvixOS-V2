#ifndef UI_APPS_CUBE_APP_H
#define UI_APPS_CUBE_APP_H

#include <stdint.h>
#include <app/app.h>

typedef struct {
    int window_id;
    int auto_rotate;
    int last_mx;
    int last_my;
    int dragging;
    uint32_t last_tick;
    int32_t spin_accum;
    int32_t orientation[9];
} cube_app_t;

extern const app_vtbl_t cube_app_vtbl;

#endif