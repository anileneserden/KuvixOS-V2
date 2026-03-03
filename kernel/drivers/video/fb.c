#include <kernel/drivers/video/fb.h>
#include <kernel/memory/kmalloc.h>
#include <lib/string.h>
#include <kernel/printk.h>
#include <stdint.h>

static uint32_t* fb_addr = 0;        // Donanım LFB (linear framebuffer)
static uint32_t* fb_backbuffer = 0;  // Packed backbuffer (width*height)

static uint32_t FB_WIDTH  = 0;
static uint32_t FB_HEIGHT = 0;

static uint32_t FB_PITCH_BYTES  = 0; // bytes per scanline
static uint32_t FB_PITCH_PIXELS = 0; // pixels per scanline (pitch/4 for 32bpp)

void fb_init(uint32_t lfb_addr, uint32_t width, uint32_t height, uint32_t pitch_bytes) {
    fb_addr = (uint32_t*)lfb_addr;

    FB_WIDTH = width;
    FB_HEIGHT = height;

    FB_PITCH_BYTES = pitch_bytes;
    FB_PITCH_PIXELS = (pitch_bytes / 4); // 32bpp varsayımı

    // Güvenlik: pitch hiç gelmediyse “packed” varsay
    if (FB_PITCH_PIXELS == 0) {
        FB_PITCH_PIXELS = FB_WIDTH;
        FB_PITCH_BYTES = FB_WIDTH * 4;
    }

    // Backbuffer'ı gerçek çözünürlükte ayır
    fb_backbuffer = (uint32_t*)kmalloc(FB_WIDTH * FB_HEIGHT * sizeof(uint32_t));

    fb_clear(0x1a1a1a);
    fb_present();

    printk("FB INIT: addr=%x w=%d h=%d pitchB=%d pitchP=%d\n",
           (uint32_t)fb_addr, FB_WIDTH, FB_HEIGHT, FB_PITCH_BYTES, FB_PITCH_PIXELS);
}

void fb_putpixel(int x, int y, uint32_t color) {
    if (!fb_backbuffer) return;
    if (x < 0 || y < 0) return;
    if ((uint32_t)x >= FB_WIDTH || (uint32_t)y >= FB_HEIGHT) return;

    fb_backbuffer[(uint32_t)y * FB_WIDTH + (uint32_t)x] = color;
}

uint32_t fb_getpixel(int x, int y) {
    if (!fb_backbuffer) return 0;
    if (x < 0 || y < 0) return 0;
    if ((uint32_t)x >= FB_WIDTH || (uint32_t)y >= FB_HEIGHT) return 0;

    return fb_backbuffer[(uint32_t)y * FB_WIDTH + (uint32_t)x];
}

void fb_clear(uint32_t color) {
    if (!fb_backbuffer) return;

    uint32_t n = FB_WIDTH * FB_HEIGHT;
    for (uint32_t i = 0; i < n; i++) {
        fb_backbuffer[i] = color;
    }
}

void fb_present(void) {
    if (!fb_addr || !fb_backbuffer) return;
    if (fb_addr == fb_backbuffer) return;

    // Donanım framebuffer pitch'i packed olmayabilir -> satır satır kopyala
    for (uint32_t y = 0; y < FB_HEIGHT; y++) {
        uint32_t* dst = fb_addr + y * FB_PITCH_PIXELS;
        uint32_t* src = fb_backbuffer + y * FB_WIDTH;
        memcpy(dst, src, FB_WIDTH * sizeof(uint32_t));
    }
}

void fb_present_rect(int x, int y, int w, int h) {
    if (!fb_addr || !fb_backbuffer) return;

    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (w <= 0 || h <= 0) return;

    if ((uint32_t)(x + w) > FB_WIDTH)  w = (int)FB_WIDTH  - x;
    if ((uint32_t)(y + h) > FB_HEIGHT) h = (int)FB_HEIGHT - y;
    if (w <= 0 || h <= 0) return;

    for (int yy = 0; yy < h; yy++) {
        uint32_t* dst = fb_addr + (uint32_t)(y + yy) * FB_PITCH_PIXELS + (uint32_t)x;
        uint32_t* src = fb_backbuffer + (uint32_t)(y + yy) * FB_WIDTH + (uint32_t)x;
        memcpy(dst, src, (uint32_t)w * sizeof(uint32_t));
    }
}

void fb_draw_rect(int x, int y, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    for (int yy = 0; yy < h; yy++) {
        for (int xx = 0; xx < w; xx++) {
            fb_putpixel(x + xx, y + yy, color);
        }
    }
}

void fb_draw_rect_outline(int x, int y, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0) return;

    for (int xx = 0; xx < w; xx++) {
        fb_putpixel(x + xx, y,         color);
        fb_putpixel(x + xx, y + h - 1, color);
    }
    for (int yy = 0; yy < h; yy++) {
        fb_putpixel(x,         y + yy, color);
        fb_putpixel(x + w - 1, y + yy, color);
    }
}

uint32_t fb_get_width(void) { return FB_WIDTH; }
uint32_t fb_get_height(void) { return FB_HEIGHT; }
uint32_t fb_get_pitch_bytes(void) { return FB_PITCH_BYTES; }
uint32_t fb_get_pitch_pixels(void) { return FB_PITCH_PIXELS; }

uint32_t fb_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return (uint32_t)((r << 16) | (g << 8) | b);
}

fb_color_t fb_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

void fb_blit_argb_key(int x, int y, int w, int h, const uint32_t* data, uint32_t key) {
    if (!data) return;
    for (int yy = 0; yy < h; yy++) {
        for (int xx = 0; xx < w; xx++) {
            uint32_t color = data[yy * w + xx];
            if (color != key) {
                fb_putpixel(x + xx, y + yy, color);
            }
        }
    }
}

uint32_t* fb_backbuffer_ptr(void) {
    return fb_backbuffer;
}

void fb_set_resolution(uint32_t width, uint32_t height) {
    // Şimdilik sadece FB_WIDTH/FB_HEIGHT'i güncelle.
    // Gerçek mod değişimi GRUB/VBE tarafında olur; kernel burada sadece
    // mevcut framebuffer parametreleriyle çalışır.
    if (width == 0 || height == 0) return;

    FB_WIDTH = width;
    FB_HEIGHT = height;

    // Güvenlik: pitch 0 ise düzelt
    if (FB_PITCH_PIXELS == 0) FB_PITCH_PIXELS = FB_WIDTH;

    fb_clear(0x1A1A1A);
    fb_present();
}
