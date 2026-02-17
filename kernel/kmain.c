// kernel/kmain.c
#include <stdint.h>
#include <multiboot2.h>

#include <kernel/serial.h>
#include <kernel/printk.h>

#include <arch/x86/gdt.h>
#include <arch/x86/idt.h>

#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/video/fb_console.h>

#include <kernel/drivers/input/keyboard.h>
#include <kernel/drivers/input/mouse_ps2.h>

#include <kernel/time.h>

// UI / Session
#include <ui/session.h>

#include <kernel/fs/fs_init.h>

extern void gdt_init(void);
extern void idt_init(void);

// ------------------------------------------------------------
// Framebuffer init (multiboot)
// ------------------------------------------------------------
static void init_framebuffer(uint32_t magic, multiboot_info_t* mbi) {
    // Multiboot1 magic: 0x2BADB002
    if (magic == 0x2BADB002 && mbi && (mbi->flags & (1 << 12))) {
        fb_init((uint32_t)mbi->framebuffer_addr);
        fb_set_resolution(mbi->framebuffer_width, mbi->framebuffer_height);
    } else {
        // fallback (qemu bochs gibi)
        fb_init(0xFD000000);
        fb_set_resolution(1024, 768);
    }
}

// ------------------------------------------------------------
// Kernel entry
// ------------------------------------------------------------
void kernel_main(uint32_t magic, multiboot_info_t* mbi) {
    serial_init();
    gdt_init();
    idt_init();

    init_framebuffer(magic, mbi);
    gfx_init();
    fb_console_init(0x00FFFFFF, 0x00000000);

    // input init
    kbd_init();
    ps2_mouse_init();

    // time
    timer_init(1000);

    // ✅ IRQ’ları aç (mouse/timer için şart)
    asm volatile("sti");

    // UI
    ui_session_init();
    ui_session_switch(UI_SESSION_DESKTOP);

    while (1) {
        // klavye event dispatch
        uint16_t sc;
        while ((sc = kbd_pop_event()) != 0) {
            ui_session_handle_scancode(sc);
        }

        // frame tick (desktop burada mouse poll yapmalı)
        ui_session_tick();

        // CPU boşta bekle (IRQ gelince uyanır)
        asm volatile("hlt");
    }
}