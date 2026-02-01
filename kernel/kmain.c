#include <stdint.h>
#include <multiboot2.h>      // Multiboot 1 standardı için
#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/serial.h>
#include <ui/desktop.h>
#include <ui/theme.h>
#include <kernel/printk.h>
#include <arch/x86/gdt.h>   // gdt_init için
#include <arch/x86/idt.h>   // idt_init için
#include <kernel/time.h>    // time_init_from_rtc ve timer_init için
#include "kernel/drivers/input/mouse_ps2.h"

// kmain.c üst kısım
extern void gdt_init(void);
extern void idt_init(void);
extern void time_init_from_rtc(void);
extern void timer_init(uint32_t freq);
// Başlık dosyası sorununu bypass etmek için buraya ekle:
extern void ps2_mouse_init(void);

void kernel_main(uint32_t magic, multiboot_info_t* mbi) {
    // 1. Temel donanımları hazırla
    serial_init();
    gdt_init();      
    idt_init();

    // 2. Görüntü sistemini başlat
    if (magic == 0x2BADB002 && (mbi->flags & (1 << 12))) {
        fb_init((uint32_t)mbi->framebuffer_addr); 
    } else {
        fb_init(0xFD000000); 
    }
    gfx_init();
    ui_theme_bootstrap_default();

    // 3. Ekranı temizle ve bilgi ver
    gfx_clear(0x1a1a1a);
    printk("KuvixOS: Sistem hazir. Zamanlayici baslatiliyor...\n");
    fb_present(); 

    // 4. Zaman sistemini kur
    time_init_from_rtc();   // BIOS'tan saati al
    timer_init(1000);       // Donanım sayacını (PIT) 1000Hz (1ms) olarak başlat!
    ps2_mouse_init();

    // 5. Kesmeleri aç (Artık Timer sinyalleri işlemciye ulaşabilir)
    asm volatile("sti"); 

    // 6. Masaüstüne gir
    ui_desktop_run(); 

    while(1) { asm volatile("hlt"); }
}