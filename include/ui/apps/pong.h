#pragma once
#include <app/app.h>
#include <stdint.h>

typedef struct {
    float p1_y, p2_y;
    float ball_x, ball_y;
    float ball_vx, ball_vy;

    int score1, score2;

    uint32_t last_ms;
    int started;

    // key states
    int p1_up, p1_down;
    int p2_up, p2_down;

    int cw, ch;
} pong_t;

extern const app_vtbl_t pong_vtbl;