#include <kernel/drivers/video/fb.h>
#include <kernel/memory/kmalloc.h>
#include <lib/string.h>
#include <kernel/printk.h>

static uint32_t* fb_addr = 0;        
static uint32_t* fb_backbuffer = 0;  

uint32_t FB_WIDTH = 0;
uint32_t FB_HEIGHT = 0;
uint32_t FB_PITCH = 0; 

void fb_init(uint32_t vbe_lfb_addr, uint32_t width, uint32_t height, uint32_t pitch) {
    fb_addr = (uint32_t*)vbe_lfb_addr;
    FB_WIDTH = width;
    FB_HEIGHT = height;
    FB_PITCH = pitch; 

    // Tek seferlik backbuffer ayırma (1920x1080 üst sınır)
    if (!fb_backbuffer) {
        fb_backbuffer = (uint32_t*)kmalloc(1920 * 1080 * sizeof(uint32_t));
    }
    
    fb_clear(0x1a1a1a);
    fb_present();
}

void fb_putpixel(int x, int y, uint32_t color) {
    if (x < 0 || (uint32_t)x >= FB_WIDTH || y < 0 || (uint32_t)y >= FB_HEIGHT) return;
    fb_backbuffer[y * FB_WIDTH + x] = color;
}

uint32_t fb_getpixel(int x, int y) {
    if (x < 0 || (uint32_t)x >= FB_WIDTH || y < 0 || (uint32_t)y >= FB_HEIGHT) return 0;
    return fb_backbuffer[y * FB_WIDTH + x];
}

void fb_clear(uint32_t color) {
    for (uint32_t i = 0; i < FB_WIDTH * FB_HEIGHT; i++) {
        fb_backbuffer[i] = color;
    }
}

void fb_present(void) {
    if (!fb_addr || !fb_backbuffer) return;
    
    // Pitch uyumlu kopyalama (Donanımdaki kaymayı çözen kısım)
    for (uint32_t y = 0; y < FB_HEIGHT; y++) {
        uint8_t* dest = (uint8_t*)fb_addr + (y * FB_PITCH);
        uint32_t* src = &fb_backbuffer[y * FB_WIDTH];
        memcpy(dest, src, FB_WIDTH * 4);
    }
}

void fb_draw_rect(int x, int y, int w, int h, uint32_t color) {
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            fb_putpixel(x + j, y + i, color);
        }
    }
}

void fb_draw_rect_outline(int x, int y, int w, int h, uint32_t color) {
    for (int xx = 0; xx < w; xx++) {
        fb_putpixel(x + xx, y,         color);
        fb_putpixel(x + xx, y + h - 1, color);
    }
    for (int yy = 0; yy < h; yy++) {
        fb_putpixel(x,         y + yy, color);
        fb_putpixel(x + w - 1, y + yy, color);
    }
}

void fb_blit_argb_key(int x, int y, int w, int h, const uint32_t* data, uint32_t key) {
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            uint32_t color = data[i * w + j];
            if (color != key) {
                fb_putpixel(x + j, y + i, color);
            }
        }
    }
}

uint32_t fb_get_width(void) { return FB_WIDTH; }
uint32_t fb_get_height(void) { return FB_HEIGHT; }
uint32_t fb_get_pitch(void) { return FB_PITCH; }

fb_color_t fb_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return (fb_color_t)((r << 16) | (g << 8) | b);
}

fb_color_t fb_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return (fb_color_t)((a << 24) | (r << 16) | (g << 8) | b);
}

void fb_set_resolution(uint32_t width, uint32_t height) {
    FB_WIDTH = width;
    FB_HEIGHT = height;
}