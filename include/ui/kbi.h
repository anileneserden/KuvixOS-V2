#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int width;
    int height;
    int palette_size;
    uint32_t palette[256];
    uint8_t* pixels;
} kbi_image_t;

int  kbi_load(const char* path, kbi_image_t* out);
void kbi_free(kbi_image_t* img);
void kbi_draw_scaled(const kbi_image_t* img, int dst_x, int dst_y, int scale);
bool kbi_is_loaded(const kbi_image_t* img);