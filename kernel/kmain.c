#include <stdint.h>
#include <multiboot2.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/serial.h>
#include <ui/desktop.h>
#include <ui/theme.h>
#include <kernel/printk.h>
#include <arch/x86/gdt.h>
#include <arch/x86/idt.h>
#include <kernel/time.h>
#include "kernel/drivers/input/mouse_ps2.h"
#include <ui/keyboard_test.h>
#include <ui/font_atlas.h>

// Eksik extern tanımları
extern void kbd_init(void);         // Klavye sürücüsü başlangıç fonksiyonu
extern void ps2_mouse_init(void);
extern void timer_init(uint32_t freq);
extern void ui_keyboard_test_run(void);

void kernel_main(uint32_t magic, multiboot_info_t* mbi) {
    // 1. Düşük Seviye Donanım & Debug Portu
    serial_init();      // Logları seri porttan görmek için
    gdt_init();         // Global Descriptor Table
    idt_init();         // Interrupt Descriptor Table (Kesme tabloları)

    // 2. Grafik ve Video Belleği (Framebuffer)
    // Multiboot'tan gelen adresi veya fallback adresini kullan
    if (magic == 0x2BADB002 && (mbi->flags & (1 << 12))) {
        fb_init((uint32_t)mbi->framebuffer_addr); 
    } else {
        fb_init(0xFD000000); 
    }
    
    gfx_init();
    ui_theme_bootstrap_default(); // UI renk paletini yükle

    // 3. Ekranı Temizle ve Kernel Loglarını Bas
    gfx_clear(0x1a1a1a); // Koyu gri arka plan
    printk("KuvixOS: Donanim baslatiliyor...\n");

    // 4. Zaman ve Giriş Aygıtları (STI öncesi son hazırlıklar)
    time_init_from_rtc();   // BIOS'tan saati çek
    timer_init(1000);       // Sistem saatini 1ms hassasiyete ayarla (PIT)
    
    // Klavye ve Mouse donanımlarını hazırla (I/O portlarını kur)
    kbd_init();             
    ps2_mouse_init();

    printk("KuvixOS: Kesmeler aciliyor. Kullanici arayuzune geciliyor...\n");
    fb_present();           // İlk logları ekrana yansıt

    // 5. Kesmeleri Aktif Et (CPU artık klavye/saat sinyallerini kabul eder)
    asm volatile("sti"); 

    // 6. Masaüstü Ortamını Başlat (Sonsuz Döngüye Girer)
    // ui_desktop_run genelde kendi içinde draw_loop barındırır
    // ui_desktop_run(); 

    // ui_keyboard_test_run(); // Doğrudan bizim testimize gir
    ui_show_font_atlas();

    // Eğer desktop'tan çıkılırsa (hata veya shutdown), CPU'yu durdur
    while(1) { 
        asm volatile("hlt"); 
    }
}