#include <ui/widgets/label.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/video/gfx.h> // Merkezi çizim için ekledik
#include <font/font8x16_tr.h>       // 8x16 sistemi için

// draw_text8 fonksiyonunu tamamen SİLDİK. 
// Çünkü artık merkezi gfx_draw_text kullanacağız.

void ui_label_init(ui_label_t* l, int x, int y, const char* text, uint32_t color)
{
    l->x = x; 
    l->y = y; 
    l->text = text; 
    l->color = color;
}

void ui_label_set_text(ui_label_t* l, const char* text)
{
    l->text = text;
}

void ui_label_draw(const ui_label_t* l)
{
    if (!l || !l->text) return;
    
    // Eski draw_text8 yerine merkezi gfx_draw_text'i çağırıyoruz.
    // Bu fonksiyon zaten 8x16 ve Türkçe desteğine sahip.
    gfx_draw_text(l->x, l->y, l->color, l->text);
}