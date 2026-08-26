// kernel/drivers/video/ttf.c
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifndef _MATH_H
#define _MATH_H
#endif
#ifndef _ASSERT_H
#define _ASSERT_H
#endif

#include <lib/math.h>

#include <kernel/memory/kmalloc.h>
#include <kernel/printk.h>
#include <kernel/memory/stubs.h>

// Bellek ve matematik eşlemeleri
#define STBTT_MALLOC(x,u)  ((void)(u), kmalloc(x))
#define STBTT_FREE(x,u)    ((void)(u), kfree(x))

#define STBTT_sqrt(x)   ((float)sqrt((float)(x)))
#define STBTT_cos(x)    ((float)cos((float)(x)))
#define STBTT_acos(x)   ((float)acos((float)(x)))
#define STBTT_pow(x,y)  (1.0f)
#define STBTT_fabs(x)   ((x) < 0 ? -(x) : (x))

// 🔹 YENİ EKLENENLER (floor, ceil, fmod yönlendirmeleri)
#define STBTT_ifloor(x) ((int)floor((float)(x)))
#define STBTT_iceil(x)  ((int)ceil((float)(x)))
#define STBTT_fmod(x,y) ((float)fmod((float)(x), (float)(y)))

#define STB_TRUETYPE_IMPLEMENTATION
#include <lib/stb_truetype.h>

static bool g_ttf_initialized = false;
static stbtt_fontinfo g_font_info;
static uint8_t* g_font_buffer = NULL;
static float g_font_size = 16.0f;
static float g_scale = 1.0f;
static int g_ascent = 0;
static int g_descent = 0;
static int g_line_gap = 0;

int ttf_is_initialized(void) {
    return g_ttf_initialized ? 1 : 0;
}

bool ttf_init_from_memory(uint8_t* font_buffer, size_t buffer_size, float pixel_height) {
    (void)buffer_size;
    if (!font_buffer) return false;

    g_font_buffer = font_buffer;
    g_font_size = pixel_height;

    // TrueType font dosyasını başlat ve parse et
    if (!stbtt_InitFont(&g_font_info, g_font_buffer, stbtt_GetFontOffsetForIndex(g_font_buffer, 0))) {
        printk("[TTF] Hata: stbtt_InitFont basarisiz oldu!\n");
        g_ttf_initialized = false;
        return false;
    }

    // Ölçek faktörünü ve metrikleri hesapla
    g_scale = stbtt_ScaleForPixelHeight(&g_font_info, g_font_size);
    stbtt_GetFontVMetrics(&g_font_info, &g_ascent, &g_descent, &g_line_gap);

    g_ttf_initialized = true;
    printk("[TTF] TrueType font basariyla baslatildi (Boyut: %.1f px)\n", pixel_height);
    return true;
}

// Belirli bir Unicode karakter kod noktası (codepoint) için alpha bitmap üretir
unsigned char* ttf_get_code_bitmap(uint32_t codepoint, int* w, int* h, int* xoff, int* yoff) {
    if (!g_ttf_initialized) return NULL;

    int glyph_index = stbtt_FindGlyphIndex(&g_font_info, (int)codepoint);
    if (glyph_index == 0) {
        // Karakter bulunamazsa soru işareti (?) dene
        glyph_index = stbtt_FindGlyphIndex(&g_font_info, '?');
        if (glyph_index == 0) return NULL;
    }

    int ix0, iy0, ix1, iy1;
    (void)ix1;
    (void)iy1;
    
    // Karakterin bitmap boyutlarını ve offsetlerini al
    unsigned char* bitmap = stbtt_GetGlyphBitmap(&g_font_info, g_scale, g_scale, glyph_index, w, h, &ix0, &iy0);
    
    if (xoff) *xoff = ix0;
    if (yoff) *yoff = iy0; // Baseline'a göre dikey ofset

    return bitmap; 
}

// Fonksiyon ismindeki mükerrer 'ttf_' düzeltildi
void ttf_free_bitmap(unsigned char* bitmap) {
    if (bitmap) {
        stbtt_FreeBitmap(bitmap, NULL);
    }
}