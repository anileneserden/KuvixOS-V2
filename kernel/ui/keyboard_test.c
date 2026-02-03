#include <kernel/drivers/video/gfx.h>
#include <stdint.h>

// Klavye sürücünden karakter okuyan fonksiyon
extern char kbd_get_key(void); 

// Basılan tuşları tutmak için bir buffer (tampon)
static char kbd_test_buffer[64] = {0};
static int kbd_idx = 0;

void ui_keyboard_test_run(void) {
    // wm_init() veya appmgr_init() çağırmıyoruz, tamamen izole çalışıyoruz.

    while(1) {
        // 1. KLAVYE OKUMA
        char c = kbd_get_key();
        if (c != 0) {
            if (c == '\b') { // Backspace (Geri silme) desteği
                if (kbd_idx > 0) {
                    kbd_idx--;
                    kbd_test_buffer[kbd_idx] = '\0';
                }
            } else if (c == '\n') { // Enter basılırsa temizle
                for(int i = 0; i < 64; i++) kbd_test_buffer[i] = 0;
                kbd_idx = 0;
            } else if (kbd_idx < 63) { // Normal karakter ekle
                kbd_test_buffer[kbd_idx++] = c;
                kbd_test_buffer[kbd_idx] = '\0';
            }
        }

        // 2. ÇİZİM (BACKBUFFER)
        fb_clear(0x182838); // Arka plan rengi
        
        // Başlık
        gfx_draw_text(10, 10, 0xFFFFFF, "KuvixOS V2 - Minimal Klavye Testi");
        
        // Klavye Durumu
        gfx_draw_text(10, 40, 0xFFFF00, "KLAVYE GIRISI:"); 
        
        if (kbd_idx == 0) {
            gfx_draw_text(130, 40, 0x555555, "Bir tusa basin...");
        } else {
            // Basılan tuşları yeşil renkle yazdır
            gfx_draw_text(130, 40, 0x00FF00, kbd_test_buffer); 
        }

        // Bilgi mesajı
        gfx_draw_text(10, 70, 0xAAAAAA, "Enter: Temizle | Backspace: Sil");

        // 3. EKRANA YANSIT (FRONTBUFFER)
        fb_present();

        // CPU'yu biraz rahatlat
        for(volatile int i = 0; i < 10000; i++); 
    }
}