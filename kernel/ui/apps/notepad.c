#include <app/app.h>
#include <app/app_manager.h>
#include <ui/wm.h>
#include <kernel/drivers/video/gfx.h>
#include <lib/string.h>
#include <stdint.h>
#include <stdbool.h>

#define NOTEPAD_MAX_TEXT 4096

// notepad.data_size (4096) kadar yer ayrılacak
typedef struct {
    char text[NOTEPAD_MAX_TEXT];
    char file_path[128];     // Mevcut dosya yolu
    char save_buffer[64];    // Diyalogda yazılan geçici isim
    uint32_t cursor;
    bool is_saving;          // Diyalog açık mı?
} notepad_data_t;

// --- DIŞ FONKSİYONLAR ---
extern char kbd_scancode_to_ascii(uint8_t scancode);

// --- VTABLE FONKSİYONLARI ---

static void notepad_on_create(app_t* self) {
    notepad_data_t* data = (notepad_data_t*)self->user;
    if (data) {
        data->cursor = 0;
        memset(data->text, 0, NOTEPAD_MAX_TEXT);
    }
}

static void notepad_on_draw(app_t* self) {
    if (!self || !self->user) return;
    notepad_data_t* data = (notepad_data_t*)self->user;

    ui_rect_t client = wm_get_client_rect(self->win_id);

    // 1. Arka plan
    gfx_fill_rect(client.x, client.y, client.w, client.h, 0xFFFFFF);
    
    // 2. Metni Satır Satır Çizme
    int curr_x = client.x + 5;
    int curr_y = client.y + 5;
    int line_height = 16; // Font yüksekliğine göre ayarla

    char line_buffer[256];
    int lb_idx = 0;

    for (uint32_t i = 0; i <= data->cursor; i++) {
        char c = data->text[i];
        
        // Eğer imleç konumundaysak imleci ekle (isteğe bağlı, sona da eklenebilir)
        if (i == data->cursor) {
            line_buffer[lb_idx++] = '_';
            line_buffer[lb_idx] = '\0';
            gfx_draw_text(curr_x, curr_y, 0x000000, line_buffer);
            break;
        }

        if (c == '\n') {
            // Satırı bitir ve çiz
            line_buffer[lb_idx] = '\0';
            gfx_draw_text(curr_x, curr_y, 0x000000, line_buffer);
            
            // Alt satıra geç
            curr_y += line_height;
            lb_idx = 0;
            
            // Sayfa dışına taşma kontrolü
            if (curr_y > client.y + client.h - line_height) break;
        } else {
            line_buffer[lb_idx++] = c;
            // Buffer taşma koruması
            if (lb_idx >= 255) {
                line_buffer[lb_idx] = '\0';
                gfx_draw_text(curr_x, curr_y, 0x000000, line_buffer);
                lb_idx = 0; // Veya satırı kaydır
            }
        }
    }
}

static void notepad_on_key(app_t* self, uint16_t scancode) {
    notepad_data_t* data = (notepad_data_t*)self->user;
    
    // ASCII çevirisi
    char c = kbd_scancode_to_ascii((uint8_t)scancode);

    // Enter kontrolü (Scancode 0x1C = Enter)
    if (scancode == 0x1C) {
        if (data->cursor < NOTEPAD_MAX_TEXT - 1) {
            data->text[data->cursor++] = '\n';
            data->text[data->cursor] = '\0';
        }
        return;
    }

    // Backspace
    if (c == '\b') {
        if (data->cursor > 0) {
            data->text[--data->cursor] = '\0';
        }
    } 
    // Yazılabilir karakterler (Space dahil)
    else if (c >= 32 && c <= 126) {
        if (data->cursor < NOTEPAD_MAX_TEXT - 1) {
            data->text[data->cursor++] = c;
            data->text[data->cursor] = '\0';
        }
    }
}

static void notepad_on_destroy(app_t* self) {
    (void)self; // Temizlik gerekirse buraya
}

// --- VTABLE TANIMI ---
const app_vtbl_t notepad_vtbl = {
    .on_create  = notepad_on_create,
    .on_draw    = notepad_on_draw,
    .on_key     = notepad_on_key,
    .on_destroy = notepad_on_destroy
};