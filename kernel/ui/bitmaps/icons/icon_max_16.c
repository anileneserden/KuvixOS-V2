#include <stdint.h>
#include <kernel/drivers/video/fb.h>
#include <ui/bitmaps/icons/icon_max_16.h>

static const uint8_t max_bitmap[ICON_MAX_H][ICON_MAX_W] = {
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,0},
    {0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,0},
    {0,1,1,0,1,1,0,0,0,0,0,0,0,1,1,0},
    {0,1,1,0,1,1,0,0,0,0,0,0,0,1,1,0},
    {0,1,1,0,1,1,0,0,0,0,0,0,0,1,1,0},
    {0,1,1,0,1,1,0,0,0,0,0,0,0,1,1,0},
    {0,1,1,0,1,1,0,0,0,0,0,0,0,1,1,0},
    {0,1,1,0,1,1,0,0,0,0,0,0,0,1,1,0},
    {0,1,1,0,1,1,0,0,0,0,0,0,0,1,1,0},
    {0,1,1,0,1,1,1,1,1,1,1,1,1,1,1,0},
    {0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0},
    {0,1,1,1,1,1,1,1,1,1,1,1,1,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
};

static const uint32_t max_palette[] = {
    0x00000000u, // 0 transparent
    0xFFFFFFFFu, // 1 white
    0xFF000000u, // 2 black
};

static uint32_t max_argb[ICON_MAX_W * ICON_MAX_H];
const uint32_t* g_icon_max_16 = max_argb;

void icon_max_16_init(void)
{
    for (int y = 0; y < ICON_MAX_H; y++) {
        for (int x = 0; x < ICON_MAX_W; x++) {
            max_argb[y * ICON_MAX_W + x] = max_palette[ max_bitmap[y][x] ];
        }
    }
}
