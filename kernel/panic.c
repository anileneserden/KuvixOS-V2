#include <kernel/panic.h>
#include <kernel/serial.h>
#include <kernel/power.h> // power_reboot için
#include <kernel/time.h>  // g_ticks_ms veya bekleme için

extern void draw_panic_screen(const char* message);

void panic(const char *message) {
    // 1. Interruptları kapat
    asm volatile ("cli");

    // 2. Logları seri porta yaz
    serial_write("\n[PANIC] ");
    serial_write(message);
    serial_write("\n");

    // 3. Mavi ekranı çiz ve ekrana bas (fb_present içermeli)
    draw_panic_screen(message);

    // 4. Kullanıcıya mesajı okuması için süre tanı (Yaklaşık 5 saniye)
    // Not: Interruptlar kapalı olduğu için zamanlayıcı (timer) çalışmayabilir.
    // Bu yüzden basit bir "busy loop" kullanıyoruz.
    serial_write("Sistem 5 saniye icinde yeniden baslatilacak...\n");
    for (volatile int i = 0; i < 500000000; i++) {
        asm volatile ("nop");
    }

    // 5. Yeniden başlat!
    serial_write("Reboot tetikleniyor...\n");
    power_reboot();

    // Eğer reboot başarısız olursa (fallback)
    while (1) {
        asm volatile ("hlt");
    }
}