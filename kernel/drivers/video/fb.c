#include <kernel/drivers/video/fb.h>
#include <kernel/memory/kmalloc.h>
#include <lib/string.h>
#include <kernel/printk.h>

static uint32_t* fb_addr = 0;        // Gerçek Video Belleği (Donanım)
static uint32_t* fb_backbuffer = 0;  // Arka Tampon (Yazılım)

uint32_t FB_WIDTH = 1920;  // Varsayılan değerler (Bootloader'dan gelenle eşleşmeli)
uint32_t FB_HEIGHT = 1080;

// fb.c içindeki fb_init kısmında en büyük boyutu ayır
void fb_init(uint32_t vbe_lfb_addr) {
    fb_addr = (uint32_t*)vbe_lfb_addr;
    
    // En büyük çözünürlük (1920x1080) kadar yeri tek seferde ayırıyoruz
    // Böylece kfree ihtiyacımız kalmıyor, hep bu alanı kullanıyoruz.
    fb_backbuffer = (uint32_t*)kmalloc(1920 * 1080 * sizeof(uint32_t));
    
    fb_clear(0x1a1a1a);
    fb_present();
}

void fb_putpixel(int x, int y, uint32_t color) {
    // Sınır kontrolü çok önemli, aksi halde kernel panic oluşur
    if (x < 0 || x >= FB_WIDTH || y < 0 || y >= FB_HEIGHT) return;
    
    // Pikseller her zaman arka tampona yazılır
    fb_backbuffer[y * FB_WIDTH + x] = color;
}

void fb_clear(uint32_t color) {
    // Tüm tamponu tek seferde boya
    for (int i = 0; i < FB_WIDTH * FB_HEIGHT; i++) {
        fb_backbuffer[i] = color;
    }
}

void fb_present(void) {
    if (!fb_addr || !fb_backbuffer || fb_addr == fb_backbuffer) return;
    
    // Arka planda hazırlanan tertemiz görüntüyü ekrana tek seferde kopyala
    memcpy(fb_addr, fb_backbuffer, FB_WIDTH * FB_HEIGHT * sizeof(uint32_t));
}

// Yardımcı Fonksiyonlar
void fb_draw_rect(int x, int y, int w, int h, uint32_t color) {
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            fb_putpixel(x + j, y + i, color);
        }
    }
}

// fb_color_t yerine doğrudan uint32_t kullanarak linker'ın kafasını karıştırmayalım
void fb_draw_rect_outline(int x, int y, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0) return;

    // Üst ve Alt kenarlar
    for (int xx = 0; xx < w; xx++) {
        fb_putpixel(x + xx, y,         color);
        fb_putpixel(x + xx, y + h - 1, color);
    }

    // Sol ve Sağ kenarlar
    for (int yy = 0; yy < h; yy++) {
        fb_putpixel(x,         y + yy, color);
        fb_putpixel(x + w - 1, y + yy, color);
    }
}

uint32_t fb_get_width(void) { return FB_WIDTH; }
uint32_t fb_get_height(void) { return FB_HEIGHT; }

uint32_t fb_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return (uint32_t)((r << 16) | (g << 8) | b);
}

// Belirli bir rengi (key) şeffaf sayarak bitmap çizer
void fb_blit_argb_key(int x, int y, int w, int h, const uint32_t* data, uint32_t key) {
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            uint32_t color = data[i * w + j];
            if (color != key) {
                fb_putpixel(x + j, y + i, color);
            }
        }
    }
}

// Renk birleştirme fonksiyonu
fb_color_t fb_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    // 32-bit ARGB formatı için (Alpha şimdilik kullanılmasa da formatı korur)
    return ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

// kernel/drivers/video/fb.c içine ekle
void fb_set_resolution(uint32_t width, uint32_t height) {
    FB_WIDTH = width;
    FB_HEIGHT = height;
    
    // Eğer kfree yoksa, fb_init'te ayırdığın büyük buffer'ı kullanmaya devam et
    // Sadece yeni boyutlara göre temizle
    fb_clear(0x1A1A1A); 
    fb_present();
}