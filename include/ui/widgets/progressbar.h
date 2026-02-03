#pragma once
#include <stdint.h>

typedef struct progressbar {
    int x, y, w, h;
    int value;          // 0..100
    int visible;        // 1/0

    uint32_t bg;        // background color (ARGB/RGBA whatever fb uses)
    uint32_t fill;      // fill color
} progressbar_t;

void progressbar_init(progressbar_t* p, int x, int y, int w, int h);
void progressbar_set_value(progressbar_t* p, int value_0_100);
void progressbar_set_colors(progressbar_t* p, uint32_t bg, uint32_t fill);
void progressbar_draw(const progressbar_t* p);
