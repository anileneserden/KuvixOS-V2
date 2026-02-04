#include <stdint.h>
#include <multiboot2.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/input/keyboard.h>
#include <arch/x86/gdt.h>
#include <arch/x86/idt.h>
#include <ui/debug_3d_render.h>
#include <ui/desktop.h>
#include <ui/ui_manager.h>

// Değişkenin gerçek tanımı burada kalsın ama tipi header ile aynı olsun
int g_current_mode = MODE_DESKTOP;

void kernel_main(uint32_t magic, multiboot_info_t* mbi) {
    gdt_init();      
    idt_init();

    // Framebuffer kurulumu
    if (magic == 0x2BADB002 && (mbi->flags & (1 << 12))) {
        fb_init((uint32_t)mbi->framebuffer_addr); 
    } else {
        fb_init(0xFD000000); 
    }
    gfx_init();
    asm volatile("sti"); 

    while(1) {
        ui_manager_update();
        asm volatile("pause");
    }
}