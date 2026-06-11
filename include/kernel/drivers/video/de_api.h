#pragma once
#include <stdint.h>

typedef struct {
    int screen_width;
    int screen_height;
    
    void (*clear)(uint32_t color);
    void (*put_pixel)(int x, int y, uint32_t color);
    
    void (*draw_rect)(int x, int y, int w, int h, uint32_t color);
    void (*draw_text)(int x, int y, const char* text, uint32_t color);
    
    void (*update_display)(void);
    void (*log)(const char* msg);
} DE_API;