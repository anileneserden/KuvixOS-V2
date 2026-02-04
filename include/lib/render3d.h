#ifndef RENDER3D_H
#define RENDER3D_H

#include <stdint.h>

typedef struct {
    int x, y, z;
} vec3_t;

typedef struct {
    int x, y;
} vec2_t;

// 3D noktayı 2D'ye çeviren sihirli fonksiyon
vec2_t project_to_screen(vec3_t v3, int width, int height);

#endif