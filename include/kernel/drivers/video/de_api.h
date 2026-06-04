#pragma once
#include <stdint.h>

typedef struct {
    int screen_width;
    int screen_height;
    void (*clear)(uint32_t color);
    void (*put_pixel)(int x, int y, uint32_t color);
    void (*update_display)(void);
    void (*log)(const char* str);
} DE_API;