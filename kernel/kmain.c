#include <stdint.h>
#include <multiboot2.h>

#include <arch/x86/gdt.h>
#include <arch/x86/idt.h>

#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/video/gfx.h>

// ✅ input manager
#include <kernel/drivers/input/input_manager.h>

// ✅ mode enum burada (MODE_DESKTOP / MODE_3D_RENDER)
#include <ui/ui_manager.h>

#include <ui/debug_3d_render.h>

// Değişkenin gerçek tanımı burada (tek bir yerde olmalı!)
int g_current_mode = MODE_3D_RENDER;

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

    // ✅ Driver init’leri (mouse/keyboard) burada tek yerden
    input_init();

    // Interrupts (timer/irq kullanıyorsan)
    asm volatile("sti");

    while (1) {
        input_update();         // ✅ önce
        debug_3d_render_loop(); // ✅ sonra
        asm volatile("pause");
    }

}
