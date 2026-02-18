#pragma once
#include <stdint.h>

typedef uint32_t fb_color_t;

void fb_init(uint32_t lfb_addr, uint32_t width, uint32_t height, uint32_t pitch_bytes);

void fb_putpixel(int x, int y, uint32_t color);
uint32_t fb_getpixel(int x, int y);

void fb_clear(uint32_t color);
void fb_present(void);
void fb_present_rect(int x, int y, int w, int h);

void fb_draw_rect(int x, int y, int w, int h, uint32_t color);
void fb_draw_rect_outline(int x, int y, int w, int h, uint32_t color);

uint32_t fb_get_width(void);
uint32_t fb_get_height(void);
uint32_t fb_get_pitch_bytes(void);
uint32_t fb_get_pitch_pixels(void);

uint32_t fb_rgb(uint8_t r, uint8_t g, uint8_t b);
fb_color_t fb_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a);

void fb_blit_argb_key(int x, int y, int w, int h, const uint32_t* data, uint32_t key);
