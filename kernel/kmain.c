// kernel/kmain.c
#include <stdint.h>
#include <multiboot2.h>   // sende bu var; multiboot_info_t kullanıyorsun (MB1)

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

    // Fallback (QEMU/Bochs gibi) — addr yoksa bir şey çizemezsin
    if (!addr || !w || !h) {
        addr  = 0xFD000000;
        w     = 1024;
        h     = 768;
        pitch = w * 4;
        bpp   = 32;
    }

    // Güvenli varsayım: 32bpp kullanıyoruz
    // (bpp 32 değilse şimdilik yine de 32 varsay, çünkü fb.c 32-bit piksel yazıyor)
    if (bpp != 32) {
        // printk ile uyarı basmak istersen aç
        // printk("[FB] Warning: bpp=%u, forcing 32bpp blit path\n", bpp);
    }

    if (pitch == 0) pitch = w * 4;

    // ✅ YENİ İMZA: addr + w/h + pitch(bytes)
    fb_init(addr, w, h, pitch);

    // Artık fb_set_resolution çağırmana gerek yok (fb_init zaten ayarlıyor)
    // ama istersen tutabilirsin; zararlı değil:
    // fb_set_resolution(w, h);
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
