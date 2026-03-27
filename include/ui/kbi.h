#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint16_t w;
    uint16_t h;
    uint32_t* pixels;   // ARGB8888: 0xAARRGGBB
} kbi_image_t;

bool kbi_load(const char* path, kbi_image_t* out);
void kbi_free(kbi_image_t* img);

// ✅ origin destekli çizim (gfx üzerinden)
void kbi_draw(const kbi_image_t* img, int x, int y);