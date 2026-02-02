#include <stdint.h>
#include <kernel/drivers/video/fb.h>
#include "icon_min_16.h"

static const uint8_t min_bitmap[ICON_MIN_H][ICON_MIN_W] = {
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,1,1,1,1,1,1,1,1,1,1,1,1,0,0},
    {0,0,1,1,1,1,1,1,1,1,1,1,1,1,0,0},
    {0,0,1,1,1,1,1,1,1,1,1,1,1,1,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
};

static const uint32_t min_palette[] = {
    0x00000000u, // 0 transparent
    0xFFFFFFFFu, // 1 white
    0xFF000000u, // 2 black
};

static uint32_t min_argb[ICON_MIN_W * ICON_MIN_H];
const uint32_t* g_icon_min_16 = min_argb;

void icon_min_16_init(void)
{
    for (int y = 0; y < ICON_MIN_H; y++) {
        for (int x = 0; x < ICON_MIN_W; x++) {
            min_argb[y * ICON_MIN_W + x] = min_palette[ min_bitmap[y][x] ];
        }
    }
}
