// src/lib/ui/input.c
#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/printk.h>
#include <ui/widgets/input.h>

//
// 1) Input event kuyruğu (ring buffer)
//

#define INPUT_QUEUE_SIZE 128

static input_event_t queue[INPUT_QUEUE_SIZE];
static volatile int head = 0;
static volatile int tail = 0;

void input_system_init(void)
{
    head = tail = 0;
}

void input_push_event(const input_event_t* ev)
{
    int next = (head + 1) % INPUT_QUEUE_SIZE;
    if (next == tail) {
        // Kuyruk dolu, şimdilik drop edelim
        return;
    }
    queue[head] = *ev;
    head = next;
}

bool input_pop_event(input_event_t* ev)
{
    if (head == tail) {
        return false; // boş
    }
    *ev = queue[tail];
    tail = (tail + 1) % INPUT_QUEUE_SIZE;
    return true;
}


//
// 2) Text input kutusu çizimi (senin mevcut kodun)
//

// Yardımcı: piksel koordinatını satır/sütuna çevir (8x16 font varsayıyorum)
static void pixel_to_rowcol(int x, int y, int* row, int* col)
{
    *col = x / 8;
    *row = y / 16;
}

void ui_input_draw(const ui_input_t* in) {
    if (!in) return;

    // 1. Kutunun İçini Çiz (Zaten fb_draw_rect kullanıyorsun, güzel)
    fb_draw_rect(in->x, in->y, in->w, in->h, fb_rgb(230, 230, 230));

    // 2. Kenarlık Rengi Belirle
    uint32_t border_color = in->has_focus ? fb_rgb(80, 120, 200) : fb_rgb(120, 120, 120);
    
    // Kenarlığı tek seferde çizmek daha performanslıdır (varsa gfx_draw_rect kullan)
    // Yoksa senin döngülerinle devam edelim:
    for (int xx = 0; xx < in->w; xx++) {
        fb_putpixel(in->x + xx, in->y, border_color);
        fb_putpixel(in->x + xx, in->y + in->h - 1, border_color);
    }
    for (int yy = 0; yy < in->h; yy++) {
        fb_putpixel(in->x, in->y + yy, border_color);
        fb_putpixel(in->x + in->w - 1, in->y + yy, border_color);
    }

    // 3. Label Çizimi (Kutunun üstündeki açıklama metni)
    if (in->label) {
        // print_move_cursor yerine gfx_draw_text
        // Renk olarak siyah (0x000000) veya koyu gri kullanabilirsin
        gfx_draw_text(in->x, in->y - 15, 0x000000, in->label);
    }

    // 4. İçerideki Metni Çiz
    if (in->buffer && in->len >= 0) {
        char tmp[256];
        int n = (in->len < 255) ? in->len : 255;

        for (int i = 0; i < n; i++) {
            tmp[i] = in->hidden ? '*' : in->buffer[i];
        }
        tmp[n] = '\0';

        // Metni kutunun içine 5 piksel padding vererek yazdır
        gfx_draw_text(in->x + 5, in->y + (in->h / 2) - 4, 0x000000, tmp);
        
        // 5. Focus Varsa Cursor (İmleç) Çiz (Küçük bir dikey çizgi)
        if (in->has_focus) {
            // Metnin genişliğini hesaplayan gfx_get_text_width gibi bir fonksiyonun yoksa 
            // şimdilik her karakteri 8 piksel varsayabiliriz:
            int cursor_x = in->x + 5 + (n * 8);
            for(int j = 0; j < 12; j++) {
                fb_putpixel(cursor_x, in->y + 5 + j, 0x000000);
            }
        }
    }
}
