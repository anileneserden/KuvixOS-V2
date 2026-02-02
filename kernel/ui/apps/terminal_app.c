#include <stdint.h>
#include <ui/wm.h>
#include <kernel/drivers/video/gfx.h>
#include <lib/string.h>

// --- Dışarıdan gelen tanımlar ---
extern const ui_rect_t* wm_get_draw_client_rect(void);
extern uint32_t g_ticks_ms; 

#define MAX_LINES 15
#define LINE_HEIGHT 16

// --- Kritik Değişkenler (En üstte tanımlanmalı) ---
static char terminal_lines[MAX_LINES][64] = {
    "KuvixOS Terminal Baglandi...",
    "Sistem hazir.",
    ""
};
static int current_line = 2;
static int char_idx = 0; // Derleyicinin "undeclared" dediği değişken burada

// Uygulama kimliği
struct {
    void* v;
    int win_id;
} terminal_app_instance;

// --- Yardımcı Fonksiyonlar ---
void terminal_newline(void) {
    if (current_line < MAX_LINES - 1) {
        current_line++;
        char_idx = 0;
        terminal_lines[current_line][0] = '\0';
    } else {
        // Kaydırma (Scroll)
        for (int i = 0; i < MAX_LINES - 1; i++) {
            memcpy(terminal_lines[i], terminal_lines[i+1], 64);
        }
        terminal_lines[MAX_LINES - 1][0] = '\0';
        char_idx = 0;
    }
}

// --- Ana Çizim ---
void terminal_on_draw_direct(void) {
    const ui_rect_t* r = wm_get_draw_client_rect();
    if (!r || r->w <= 0 || r->h <= 0) return;

    // 1. Arka planı boya
    gfx_fill_rect(r->x, r->y, r->w, r->h, 0x000000);

    // 2. Satırları çiz
    int line_height = 16;
    for (int i = 0; i <= current_line; i++) {
        int ty = r->y + 5 + (i * line_height);
        
        // Pencere dışına taşma kontrolü
        if (ty + line_height > r->y + r->h) break;

        // Komut satırı göstergesi ve metin
        if (i == current_line) {
            gfx_draw_text(r->x + 5, ty, 0x00FF00, "> "); // Yeşil prompt
            gfx_draw_text(r->x + 25, ty, 0xFFFFFF, terminal_lines[i]);
            
            // 3. Yanıp sönen imleç (500ms aralıkla)
            if ((g_ticks_ms / 500) % 2 == 0) {
                int cursor_x = r->x + 25 + (strlen(terminal_lines[i]) * 8);
                gfx_fill_rect(cursor_x, ty, 8, 14, 0xFFFFFF);
            }
        } else {
            // Eski satırlar (biraz daha sönük gri)
            gfx_draw_text(r->x + 5, ty, 0xAAAAAA, terminal_lines[i]);
        }
    }
}

void terminal_handle_key(char c) {
    if (c == '\b') {
        if (char_idx > 0) {
            char_idx--;
            terminal_lines[current_line][char_idx] = '\0';
        }
    } 
    else if (c == '\n' || c == '\r') {
        terminal_newline();
    }
    else if (char_idx < 60 && c >= 32) {
        terminal_lines[current_line][char_idx++] = c;
        terminal_lines[current_line][char_idx] = '\0';
    }
}

void terminal_putc(char c) { (void)c; }