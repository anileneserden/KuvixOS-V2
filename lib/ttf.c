#define STBTT_STATIC
#define STBTT_MALLOC(x,u)    kmalloc(x)
#define STBTT_free(x,u)      kfree(x)
#define STBTT_assert(x)      ((void)0)

// Eğer matematik fonksiyonlarına (floor, ceil, sqrt vb.) ihtiyaç duyarsa 
// kendi minik math helper'larını buraya bağlayabiliriz veya stb'nin bazı macro'larını kapatabiliriz.
#include <lib/stb_truetype.h>
#include <kernel/memory/kmalloc.h>
#include <kernel/printk.h>

static uint8_t* g_ttf_buffer = 0;
static stbtt_fontinfo g_font_info;
static int g_ttf_initialized = 0;

int ttf_init_from_memory(const uint8_t* buffer, uint32_t size) {
    g_ttf_buffer = (uint8_t*)buffer;
    
    if (!stbtt_InitFont(&g_font_info, g_ttf_buffer, stbtt_GetFontOffsetForIndex(g_ttf_buffer, 0))) {
        printk("Hata: TTF fontu baslatilamadi!\n");
        return 0;
    }
    
    g_ttf_initialized = 1;
    printk("TTF fontu basariyla yuklendi ve baslatildi!\n");
    return 1;
}