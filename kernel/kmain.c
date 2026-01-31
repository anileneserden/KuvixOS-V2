#include <stdint.h>
#include <multiboot2.h>      // Multiboot 1 standardı için
#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/serial.h>
#include <ui/desktop.h>
#include <ui/theme.h>
#include <kernel/printk.h>

void kernel_main(uint32_t magic, multiboot_info_t* mbi) {
    serial_init();
    
    // 1. Framebuffer'ı başlat
    if (magic == 0x2BADB002 && (mbi->flags & (1 << 12))) {
        fb_init((uint32_t)mbi->framebuffer_addr); 
    } else {
        fb_init(0xFD000000); 
    }

    // 2. Grafik ve Tema Sistemini Hazırla
    gfx_init();
    ui_theme_bootstrap_default();

    // 3. Ekranı temizle (Kırmızı ekrandan kurtulmak için)
    gfx_clear(0x1a1a1a); 
    fb_present(); 

    printk("KuvixOS Masaustu Yukleniyor...\n");

    // 4. Gerçek Masaüstü Döngüsüne Gir
    // Bu fonksiyon kendi içinde pencereleri çizecektir.
    ui_desktop_run(); 

    while(1) { asm volatile("hlt"); }
}