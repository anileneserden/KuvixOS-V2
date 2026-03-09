#pragma once

#include <app/app.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t fx; // 16.16 fixed

typedef struct {
    // fixed-point state
    fx posX, posY;
    fx velX, velY;

    // tuning (fixed-point)
    fx gravity;      // e.g. 0.60
    fx jumpImpulse;  // e.g. -12.00

    // movement (ints -> converted)
    int accelX;      // pixels/sec-ish baseline
    int maxVelX;     // clamp

    int friction;    // integer friction per frame-ish (we apply in fixed)
    int grounded;

    int playerW, playerH;

    uint32_t last_ms;

    uint8_t key_down[128];
    uint8_t key_prev[128];
} physics_lab_t;

extern const app_vtbl_t physics_lab_vtbl;

#ifdef __cplusplus
}
#endif