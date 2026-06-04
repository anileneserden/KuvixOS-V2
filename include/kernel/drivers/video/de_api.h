#pragma once
#include <stdint.h>

typedef struct {
    void (*clear)(uint32_t color);
    void (*put_pixel)(int x, int y, uint32_t color);
    void (*update_display)(void);
    void (*log)(const char* str);
} DE_API;