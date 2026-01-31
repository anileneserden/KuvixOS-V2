// src/bitmaps/cursor.h
#pragma once

#include <stdint.h>
#include <kernel/drivers/video/fb.h>   // fb_color_t için, path'i gerekirse düzelt

// Bitmap boyutları (cursor.c ile aynı olmalı)
#define CURSOR_W  50
#define CURSOR_H  50

// Sadece çizim fonksiyonunu dışarı açıyoruz
void cursor_draw_arrow(int x, int y);
