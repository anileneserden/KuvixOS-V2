#pragma once

#include <app/app.h>

typedef struct {
    int win_id;
    int cube_x;
    int cube_y;
    int cube_size;
    int vel_y;
    int update_counter;
    int initialized;
} game_engine_window_t;

extern const app_vtbl_t game_engine_window_vtbl;