#include <stdint.h>
#include <kernel/drivers/video/fb.h>  // sadece include path bozulmasın diye, şart değil
#include <ui/bitmaps/icons/icon_close_16.h>
#include <kernel/printk.h>

// 0 = transparent, 1 = red, 2 = black (istersen değiştir)
static const uint8_t close_bitmap[ICON_CLOSE_H][ICON_CLOSE_W] = {
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,1,1,0,0,0,0,0,0,0,0,1,1,0,0},
    {0,1,1,1,1,0,0,0,0,0,0,1,1,1,1,0},
    {0,1,1,1,1,1,0,0,0,0,1,1,1,1,1,0},
    {0,0,1,1,1,1,1,0,0,1,1,1,1,1,0,0},
    {0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0},
    {0,0,0,0,1,1,1,1,1,1,1,1,0,0,0,0},
    {0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0},
    {0,0,0,0,0,1,1,1,1,1,1,0,0,0,0,0},
    {0,0,0,0,1,1,1,1,1,1,1,1,0,0,0,0},
    {0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0},
    {0,0,1,1,1,1,1,0,0,1,1,1,1,1,0,0},
    {0,1,1,1,1,1,0,0,0,0,1,1,1,1,1,0},
    {0,1,1,1,1,0,0,0,0,0,0,1,1,1,1,0},
    {0,0,1,1,0,0,0,0,0,0,0,0,1,1,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
};

static const uint32_t close_palette[] = {
    0x00000000u, // 0 transparent (key)
    0x00FFFFFFu, // 1 white
};

static uint32_t close_argb[ICON_CLOSE_W * ICON_CLOSE_H];
const uint32_t* g_icon_close_16 = close_argb;

void icon_close_16_init(void)
{
    printk("[ICON] close init enter bitmap(1,2)=%d\n", close_bitmap[1][2]);
    for (int y = 0; y < ICON_CLOSE_H; y++) {
        for (int x = 0; x < ICON_CLOSE_W; x++) {
            close_argb[y * ICON_CLOSE_W + x] = close_palette[ close_bitmap[y][x] ];
        }
    }
    printk("[ICON] close init exit argb[10]=0x%x\n", (unsigned)close_argb[10]);
}
