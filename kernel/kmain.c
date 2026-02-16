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

#ifdef KUVIX_KBD_DEBUG
#include <kernel/debug/debug_kbd.h>
#endif

#include <lib/shell.h>

#include <kernel/drivers/ata_pio.h>

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
    serial_init();
    gdt_init();
    idt_init();

    init_framebuffer(magic, mbi);
    gfx_init();
    fb_console_init(0x00FFFFFF, 0x00000000);

    // ✅ ATA init
    ata_pio_init();
    ata_pio_print_info();
    fb_console_flush();

    kbd_init();
    shell_init();

    while (1) asm volatile("hlt");
}
