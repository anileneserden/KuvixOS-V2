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

#include <ui/session.h>
#include <app/app_manager.h>

extern void gdt_init(void);
extern void idt_init(void);

static void init_framebuffer(uint32_t magic, multiboot_info_t* mbi) {
    if (magic == 0x2BADB002 && mbi && (mbi->flags & (1 << 12))) {
        fb_init((uint32_t)mbi->framebuffer_addr);
        fb_set_resolution(mbi->framebuffer_width, mbi->framebuffer_height);
    } else {
        fb_init(0xFD000000);
        fb_set_resolution(1024, 768);
    }
}

void kernel_main(uint32_t magic, multiboot_info_t* mbi) {

    // ----------------------------------------------------
    // LOW LEVEL INIT
    // ----------------------------------------------------
    serial_init();
    gdt_init();
    idt_init();

    init_framebuffer(magic, mbi);
    gfx_init();

    fb_console_init(0x00FFFFFF, 0x00000000);

    printk("KuvixOS Desktop Starting...\n");
    fb_console_flush();

    // ----------------------------------------------------
    // INPUT INIT
    // ----------------------------------------------------
    kbd_init();
    ps2_mouse_init();

    // Interruptları aç
    asm volatile("sti");

    // ----------------------------------------------------
    // UI + APP SYSTEM INIT
    // ----------------------------------------------------
    appmgr_init();
    ui_session_init();

    // 🔥 Default session: Desktop
    ui_session_switch(UI_SESSION_DESKTOP);

    // ----------------------------------------------------
    // MAIN LOOP
    // ----------------------------------------------------
    while (1) {

        uint16_t ev;

        // --- Klavye eventlerini tüket ---
        while ((ev = kbd_pop_event()) != 0) {
            ui_session_handle_scancode(ev);
        }

        // --- Mouse update ---
        ps2_mouse_update();

        // --- Aktif session tick ---
        ui_session_tick();

        // CPU idle
        asm volatile("hlt");
    }
}