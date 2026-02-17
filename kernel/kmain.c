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
#include <kernel/kbd.h>

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

static int handle_session_hotkeys(uint8_t sc) {
    if (sc & 0x80) return 0; // break ignore

    uint8_t mods = kbd_mods();
    int ctrl = (mods & 2) != 0;
    int alt  = (mods & 4) != 0;
    if (!(ctrl && alt)) return 0;

    switch (sc) {
        case 0x3B: /* F1 */ ui_session_switch(UI_SESSION_TTY1);     return 1;
        case 0x3C: /* F2 */ ui_session_switch(UI_SESSION_DESKTOP);  return 1;
        default: return 0;
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

    printk("KuvixOS v2 starting...\n");
    fb_console_flush();

    // ----------------------------------------------------
    // INPUT INIT
    // ----------------------------------------------------
    kbd_init();
    ps2_mouse_init();

    // 🔥 Interruptları aç
    asm volatile("sti");

    // ----------------------------------------------------
    // SESSION INIT
    // ----------------------------------------------------
    ui_session_init();
    ui_session_switch(UI_SESSION_TTY1);

    // ----------------------------------------------------
    // MAIN LOOP (IRQ driven)
    // ----------------------------------------------------
    while (1) {

        uint16_t ev;

        // Kuyruktaki tüm klavye eventlerini tüket
        while ((ev = kbd_pop_event()) != 0) {
            uint8_t sc = (uint8_t)ev;

            if (handle_session_hotkeys(sc)) {
                continue;
            }

            ui_session_handle_scancode(ev);
        }

        // Aktif session çizimi
        ui_session_tick();

        // 🔥 CPU boşta uyusun, IRQ gelince uyanacak
        asm volatile("hlt");
    }
}