#include <init/session.h>
#include <lib/shell.h>

void session_init(void) {
    // Şimdilik oturum başlarken ekstra yapılması gereken bir donanım ayarı yok.
}

void session_handle_scancode(uint16_t scancode) {
    // Donanımdan (kernel_main) gelen tuş kodunu doğrudan kabuğa (Shell) üflüyoruz.
    // İleride F1, F2 gibi tuşlarla çoklu TTY yaparsan, hangi shell'e gideceğini burası seçecek.
    shell_handle_scancode(scancode);
}

void session_tick(void) {
    // Kabuktaki g_dirty (ekran değişti) bayrağını dinleyip 
    // ekranı tazeleyen tick fonksiyonunu buraya bağlıyoruz.
    shell_tick();
}