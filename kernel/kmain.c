#include <stdint.h>
#include <multiboot2.h>

#include <kernel/serial.h>
#include <kernel/printk.h>

#include <arch/x86/gdt.h>
#include <arch/x86/idt.h>

#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/video/fb_console.h>

#include <kernel/time.h>
#include <kernel/drivers/input/mouse_ps2.h>

#include <lib/shell.h>

extern void gdt_init(void);
extern void idt_init(void);
extern void time_init_from_rtc(void);
extern void timer_init(uint32_t freq);
extern void ps2_mouse_init(void);

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
    // 1) Temel init
    serial_init();
    gdt_init();
    idt_init();

    // 2) Video init
    init_framebuffer(magic, mbi);
    gfx_init();

    // 3) Konsol (framebuffer üstüne yazı basma)
    fb_console_init(0x00FFFFFF, 0x00000000);

    // Artık printk hem serial'e hem ekrana düşmeli
    printk("KuvixOS: Shell mod basliyor...\n");

    // 4) Zaman + input
    time_init_from_rtc();
    timer_init(1000);
    ps2_mouse_init();

    asm volatile("sti");

    // 5) Shell
    shell_init();

    while (1) { asm volatile("hlt"); }
}
