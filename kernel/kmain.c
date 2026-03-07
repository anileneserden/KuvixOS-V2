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

#include <kernel/memory/kmalloc.h>

#include <ui/session.h>

#include <kernel/fs/fs_init.h>

#include <ui/inputtest.h>

extern void gdt_init(void);
extern void idt_init(void);

extern uint8_t _end;

static inline uintptr_t align_up(uintptr_t x, uintptr_t a) {
    return (x + (a - 1)) & ~(a - 1);
}

// ------------------------------------------------------------
// Framebuffer init (multiboot1)
// ------------------------------------------------------------
static void init_framebuffer(uint32_t magic, multiboot_info_t* mbi) {
    uint32_t addr  = 0;
    uint32_t w     = 0;
    uint32_t h     = 0;
    uint32_t pitch = 0;   // bytes
    uint32_t bpp   = 32;

    // Multiboot1 magic: 0x2BADB002
    if (magic == 0x2BADB002 && mbi && (mbi->flags & (1 << 12))) {
        addr  = (uint32_t)mbi->framebuffer_addr;
        w     = (uint32_t)mbi->framebuffer_width;
        h     = (uint32_t)mbi->framebuffer_height;
        pitch = (uint32_t)mbi->framebuffer_pitch; // bytes per line
        bpp   = (uint32_t)mbi->framebuffer_bpp;
    }

    // Fallback (QEMU/Bochs gibi)
    if (!addr || !w || !h) {
        addr  = 0xFD000000;
        w     = 1024;
        h     = 768;
        pitch = w * 4;
        bpp   = 32;
    }

    if (pitch == 0) pitch = w * 4;

    printk("MBI flags=0x%x\n", mbi ? mbi->flags : 0);

    fb_init(addr, w, h, pitch);
}

// ------------------------------------------------------------
// Kernel entry
// ------------------------------------------------------------
void kernel_main(uint32_t magic, multiboot_info_t* mbi) {
    printk("MB magic=%x\n", magic);
    serial_init();

    gdt_init();
    idt_init();

    uintptr_t heap_base = align_up((uintptr_t)&_end, 0x1000);

    uint32_t heap_size = 32u * 1024u * 1024u;

    kmalloc_init((void*)heap_base, heap_size);

    printk("[KMALLOC] _end=%x heap_base=%x heap_size=%x KB\n",
       (uint32_t)(uintptr_t)&_end,
       (uint32_t)heap_base,
       (uint32_t)(heap_size / 1024));

    // =========================================================

    init_framebuffer(magic, mbi);

    gfx_init();
    fb_console_init(0x00FFFFFF, 0x00000000);

    kbd_init();
    ps2_mouse_init();

    timer_init(1000);

    asm volatile("sti");
    
    fs_init_once();

    // UI
    ui_session_init();
    ui_session_switch(UI_SESSION_DESKTOP);

    while (1) {
        // klavye event dispatch
        uint16_t sc;
        while ((sc = kbd_pop_event()) != 0) {
            ui_session_handle_scancode(sc);
        }

        // frame tick
        ui_session_tick();

        asm volatile("hlt");
    }
}