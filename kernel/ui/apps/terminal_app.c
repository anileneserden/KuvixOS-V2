#include <app/app.h>
#include <ui/wm.h>
#include <kernel/drivers/video/gfx.h>
#include <lib/string.h>
#include <stdint.h>
#include <kernel/printk.h>
#include <lib/commands.h>

// --- TERMINAL YAPILANDIRMASI ---
#define TERM_ROWS 20        // Ekranda görünecek maksimum satır sayısı
#define TERM_COLS 80        // Her satırın maksimum karakter sayısı
#define LINE_HEIGHT 16      // Her satırın piksel yüksekliği

static char term_history[TERM_ROWS][TERM_COLS];
static int  current_row = 0;
static char term_buffer[64] = {0};
static int  term_ptr = 0;



// Terminal ekranına yeni bir satır ekler (Yukarı kaydırma destekli)
void terminal_print(const char* text) {
    if (!text) return;

    // --- ÖZEL KONTROL: Eğer metin \f içeriyorsa ekranı temizle ---
    if (text[0] == '\f') {
        for (int i = 0; i < TERM_ROWS; i++) {
            memset(term_history[i], 0, TERM_COLS);
        }
        current_row = 0;
        return; // İşlem tamam, geri dön
    }

    static char line_buffer[TERM_COLS];
    static int line_ptr = 0;

    for (int i = 0; text[i] != '\0'; i++) {
        if (text[i] == '\n') {
            // Satırı bitir ve geçmişe ekle
            line_buffer[line_ptr] = '\0';
            if (line_ptr > 0) {
                if (current_row < TERM_ROWS - 1) {
                    strncpy(term_history[current_row++], line_buffer, TERM_COLS);
                } else {
                    // Kaydır
                    for (int j = 0; j < TERM_ROWS - 1; j++)
                        memcpy(term_history[j], term_history[j+1], TERM_COLS);
                    strncpy(term_history[TERM_ROWS - 1], line_buffer, TERM_COLS);
                }
            }
            line_ptr = 0;
            memset(line_buffer, 0, TERM_COLS);
        } else if (line_ptr < TERM_COLS - 1) {
            line_buffer[line_ptr++] = text[i];
        }
    }
}

// --- YARDIMCI FONKSİYONLAR ---

void terminal_test_turkish_chars() {
    terminal_print("--- Turkce Karakter Testi ---");
    
    // Senin trq.c dosyasındaki haritalamaya göre kodlar:
    // ğ=1, ş=2, ı=3, ö=4, ç=5, ü=6
    // Ğ=7, Ş=8, İ=9, Ö=10, Ç=11, Ü=12
    
    char test_str[] = { 'g', 1, ' ', 's', 2, ' ', 'i', 3, ' ', 'o', 4, ' ', 'c', 5, ' ', 'u', 6, '\0' };
    terminal_print("Kucukler: g(g) s(s) i(i) o(o) c(c) u(u)"); 
    terminal_print(test_str);
    
    char test_upper[] = { 7, 8, 9, 10, 11, 12, '\0' };
    terminal_print("Buyukler:");
    terminal_print(test_upper);
}

// --- APP VTABLE FONKSİYONLARI ---

void terminal_on_create(app_t* app) {
    (void)app;
    // DİKKAT: Bu satır olmazsa printk terminale akmaz!
    extern void printk_set_hook(void (*hook)(const char*));
    printk_set_hook(terminal_print);
    terminal_test_turkish_chars();
    // Açılış mesajı
    terminal_print("KuvixOS Terminal v1.1");
    terminal_print("Yardim icin 'help' yazin.");
    terminal_print("-----------------------");
}

void terminal_on_draw(app_t* app) {
    if (!app) return;
    ui_rect_t client = wm_get_client_rect(app->win_id);
    gfx_fill_rect(client.x, client.y, client.w, client.h, 0xFF000000);

    int start_y = client.y + 10;
    int line_spacing = 14; // Satır aralığını daraltalım

    // Geçmişi çiz
    for (int i = 0; i < current_row; i++) {
        // Yazının pencerenin altına taşmamasını kontrol et
        if (start_y + (i * line_spacing) > client.y + client.h - 30) break;
        
        gfx_draw_text(client.x + 10, start_y + (i * line_spacing), 0xFF00FF00, term_history[i]);
    }

    // Input satırı (Her zaman en altta kalsın)
    char display[128] = "> ";
    strcat(display, term_buffer);
    strcat(display, "_");
    gfx_draw_text(client.x + 10, client.y + client.h - 20, 0xFFFFFFFF, display);
}

void terminal_on_key(app_t* app, uint16_t key) {
    if (!app) return;

    // 1. ENTER: Komut çalıştırma
    if (key == '\n' || key == 13) {
        if (term_ptr > 0) {
            char log_cmd[128] = "> ";
            strcat(log_cmd, term_buffer);
            terminal_print(log_cmd);

            commands_execute(term_buffer);

            term_ptr = 0;
            memset(term_buffer, 0, sizeof(term_buffer));
        } else {
            terminal_print("> ");
        }
    } 
    // 2. BACKSPACE: Silme işlemi
    // Sadece buffer boş değilse (term_ptr > 0) işlem yapar.
    else if (key == 8 || key == '\b') {
        if (term_ptr > 0) {
            term_buffer[--term_ptr] = '\0';
        }
        // Buffer zaten boşsa hiçbir şey yapma ve fonksiyondan çık
    } 
    // 3. KARAKTER GİRİŞİ: Yazma işlemi
    // KRİTİK: key != 8 kontrolü eklendi, böylece backspace yanlışlıkla karakter olarak basılmaz.
    else if (key != 8 && (((key >= 32 && key <= 126) || (key >= 14 && key <= 25))) && term_ptr < 60) {
        term_buffer[term_ptr++] = (char)key;
        term_buffer[term_ptr] = '\0';
    }
}

void terminal_on_mouse(app_t* app, int mx, int my, uint8_t pressed, uint8_t released, uint8_t buttons) {
    (void)app; (void)mx; (void)my; (void)pressed; (void)released; (void)buttons;
}

// --- VTABLE TANIMI ---
const app_vtbl_t terminal_vtbl = {
    .on_create  = terminal_on_create,
    .on_draw    = terminal_on_draw,
    .on_mouse   = terminal_on_mouse,
    .on_key     = terminal_on_key,
    .on_destroy = 0
};