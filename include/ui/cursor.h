#ifndef CURSOR_H_HEADER
#define CURSOR_H_HEADER

#include <stdint.h>

// Temel ok imleci boyutları
#define CURSOR_W 20
#define CURSOR_H 20

// Fonksiyon Prototipleri
void cursor_draw_arrow(int x, int y);
void cursor_draw_resize_nwse(int x, int y);
void cursor_draw_resize_we(int x, int y);
void cursor_draw_resize_ns(int x, int y);
void cursor_draw_resize_nesw(int x, int y);

// İhtiyaç duyulursa genel çizim fonksiyonu dışarıya açılabilir
void cursor_draw_generic(int x, int y, int w, int h, const uint8_t* bitmap);

#endif