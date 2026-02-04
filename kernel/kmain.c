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

// kmain.c üst kısımlar
extern void gdt_init(void);
extern void idt_init(void);
extern void time_init_from_rtc(void);
// BURAYI KONTROL ET:
extern void timer_init(uint32_t freq); 
extern void ps2_mouse_init(void);
extern void ui_desktop_run(void);
extern void debug_screen_init(void); // Eğer debug ekranını kullanacaksan

// Eğer hala debug ekranını kullanacaksan bunu ekle:
#include <ui/debug_screen.h>

void kernel_main(uint32_t magic, multiboot_info_t* mbi) {
    serial_init();
    gdt_init();      
    idt_init();

    if (magic == 0x2BADB002 && (mbi->flags & (1 << 12))) {
        fb_init((uint32_t)mbi->framebuffer_addr); 
    } else {
        fb_init(0xFD000000); 
    }
    gfx_init();
    ui_theme_bootstrap_default();

    gfx_clear(0x1a1a1a);
    printk("KuvixOS: Sistem Hazir.\n");
    fb_present(); 

    time_init_from_rtc();   
    timer_init(1000);       
    ps2_mouse_init();

    asm volatile("sti"); 

    // --- SEÇİMİNİ YAP ---
    
    // 1. GERÇEK MASAÜSTÜNE GİTMEK İÇİN:
    // ui_desktop_run(); 

    // 2. VEYA DEBUG EKRANINDA KALMAK İÇİN:
    debug_screen_init();

    while(1) { asm volatile("hlt"); }
}