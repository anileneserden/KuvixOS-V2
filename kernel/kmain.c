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
#include <kernel/user.h> 
#include <ui/theme.h>

extern void gdt_init(void);
extern void idt_init(void);
extern uint8_t _end;

static inline uintptr_t align_up(uintptr_t x, uintptr_t a) {
    return (x + (a - 1)) & ~(a - 1);
}

static void init_framebuffer(uint32_t magic, multiboot_info_t* mbi) {
    uint32_t addr = 0, w = 0, h = 0, pitch = 0;
    if (magic == 0x2BADB002 && mbi && (mbi->flags & (1 << 12))) {
        addr = (uint32_t)mbi->framebuffer_addr;
        w = (uint32_t)mbi->framebuffer_width;
        h = (uint32_t)mbi->framebuffer_height;
        pitch = (uint32_t)mbi->framebuffer_pitch;
    }
    if (!addr) { addr = 0xFD000000; w = 1024; h = 768; pitch = w * 4; }
    fb_init(addr, w, h, pitch);
}

void kernel_main(uint32_t magic, multiboot_info_t* mbi) {
    serial_init();
    gdt_init();
    idt_init();

    uintptr_t heap_base = align_up((uintptr_t)&_end, 0x1000);
    kmalloc_init((void*)heap_base, 32u * 1024u * 1024u);

    init_framebuffer(magic, mbi);
    gfx_init();
    fb_console_init(0x00FFFFFF, 0x00000000);

    kbd_init();
    ps2_mouse_init();
    timer_init(1000);
    time_init_from_rtc();
    fs_init_once();

    user_init(); // users.cfg'yi oku
    ui_theme_bootstrap_default();
    ui_session_init();
    ui_session_switch(UI_SESSION_TTY1);

    asm volatile("sti"); // Kesmeleri aç

    while (1) {
        uint16_t ev;
        while ((ev = kbd_pop_event()) != 0) {
            uint8_t sc = (uint8_t)(ev & 0xFF);
            
            // ✅ Sadece "Make" (basılma) kodlarını gönder, "Break" (bırakma) kodlarını yoksay
            // 0x80 bit'i set edilmişse tuş bırakılmıştır.
            if (sc & 0x80) continue; 

            // F1 / F2 kontrolü
            if (sc == 0x3B) { ui_session_switch(UI_SESSION_TTY1); continue; }
            if (sc == 0x3C) { ui_session_switch(UI_SESSION_DESKTOP); continue; }

            ui_session_handle_scancode(ev);
        }

        ui_session_tick();
        asm volatile("hlt");
    }
}