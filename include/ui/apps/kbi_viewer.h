#pragma once
#include <stdint.h>

#define KBI_VIEWER_PATH_MAX 192

typedef struct {
    // image
    uint16_t w;
    uint16_t h;
    uint32_t* pixels; // ARGB8888 (A<<24 | R<<16 | G<<8 | B)

    // viewer state
    char path[KBI_VIEWER_PATH_MAX];

    int loaded;
    int failed;

    // pan
    int pan_x;
    int pan_y;

    int dragging;
    int drag_last_x;
    int drag_last_y;

    // cache client size (optional)
    int cw;
    int ch;
} kbi_viewer_t;

extern const struct app_vtbl kbi_viewer_vtbl;