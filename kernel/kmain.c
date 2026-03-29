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

#include <kernel/drivers/net/net.h>

#include <kernel/time.h>
#include <kernel/memory/kmalloc.h>

#include <kernel/fs/fs_init.h>
#include <kernel/system/seed_files.h>

#include <ui/session.h>
#include <ui/theme.h>

#ifdef KUVIX_KBD_DEBUG
#include <kernel/debug/debug_kbd.h>
#endif

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
    uint32_t pitch = 0;
    uint32_t bpp   = 32;

    if (magic == 0x2BADB002 && mbi && (mbi->flags & (1 << 12))) {
        addr  = (uint32_t)mbi->framebuffer_addr;
        w     = (uint32_t)mbi->framebuffer_width;
        h     = (uint32_t)mbi->framebuffer_height;
        pitch = (uint32_t)mbi->framebuffer_pitch;
        bpp   = (uint32_t)mbi->framebuffer_bpp;
    }

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
    serial_init();
    printk("MB magic=0x%x\n", magic);

    gdt_init();
    idt_init();

    // --------------------------------------------------------
    // Heap init (KRITIK)
    // --------------------------------------------------------
    uintptr_t heap_base = align_up((uintptr_t)&_end, 0x1000);
    uint32_t heap_size = 32u * 1024u * 1024u;
    kmalloc_init((void*)heap_base, heap_size);

    printk("[KMALLOC] _end=0x%x heap_base=0x%x heap_size=0x%x KB\n",
        (uint32_t)(uintptr_t)&_end,
        (uint32_t)heap_base,
        (uint32_t)(heap_size / 1024));

    // --------------------------------------------------------
    // Grafik
    // --------------------------------------------------------
    init_framebuffer(magic, mbi);
    gfx_init();
    fb_console_init(0x00FFFFFF, 0x00000000);

    // --------------------------------------------------------
    // Input
    // --------------------------------------------------------
    kbd_init();
    ps2_mouse_init();

    // --------------------------------------------------------
    // Net / time
    // --------------------------------------------------------
    net_init();

    timer_init(1000);
    time_set_tz_offset_sec(3 * 3600);
    time_init_from_rtc();

    // --------------------------------------------------------
    // FS
    // --------------------------------------------------------
    fs_init_once();
    seed_files_run();

    // --------------------------------------------------------
    // Theme + UI session
    // --------------------------------------------------------
    ui_theme_bootstrap_default();
    ui_session_init();

#ifdef KUVIX_KBD_DEBUG
    asm volatile("sti");
    debug_kbd_run();
#else
    ui_session_switch(UI_SESSION_TTY1);

    // KRITIK: interruptlari ac
    asm volatile("sti");

    while (1) {
        uint16_t sc;

        while ((sc = kbd_pop_event()) != 0) {
            ui_session_handle_scancode(sc);
        }

        ui_session_tick();

        asm volatile("hlt");
    }
#endif

    while (1) {
        asm volatile("hlt");
    }
}