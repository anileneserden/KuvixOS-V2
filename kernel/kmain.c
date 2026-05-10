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
#include <kernel/drivers/loader/loader.h> // Yeni eklenen
#include <kernel/fs/kvxfs.h>              // Yeni eklenen

#include <ui/session.h>

#include <kernel/fs/fs_init.h>

#include <kernel/system/seed_files.h>

#include <ui/theme/theme.h>

// ------------------------------------------------------------
// Sürücü Yükleyici İçin Ön Hazırlık
// ------------------------------------------------------------
unsigned char hello_kmod[] = {
    // ELF Header (64-byte)
    0x7f, 0x45, 0x4c, 0x46, 0x02, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x3e, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x01, 0x00, 0x00, 0x00,

    // Section Header (64-byte) - Kodun yerini (0x80) gösteriyor
    0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

    // --- GERÇEK MAKİNE KODU (0x80 Ofsetinde) ---
    0xC3, 0x90, 0x90, 0x90, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};
unsigned int hello_kmod_len = sizeof(hello_kmod);

// kmain.c içinde
void loader_bootstrap(void) {
    // exists kontrolünü şimdilik yorum satırı yapalım
    // if (!kvxfs_exists("/persist/hello.kmod")) {
        printk("[Loader] hello.kmod güncelleniyor...\n");
        kvxfs_write_all("/persist/hello.kmod", hello_kmod, hello_kmod_len);
    // }
}

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
    printk("KuvixOS V2 Baslatiliyor...\n");
    printk("MB magic=%x\n", magic);

    gdt_init();
    idt_init();

    // Bellek Yönetimi (Heap)
    uintptr_t heap_base = align_up((uintptr_t)&_end, 0x1000);
    uint32_t heap_size = 32u * 1024u * 1024u;
    kmalloc_init((void*)heap_base, heap_size);

    printk("[KMALLOC] _end=%x heap_base=%x heap_size=%x KB\n",
       (uint32_t)(uintptr_t)&_end,
       (uint32_t)heap_base,
       (uint32_t)(heap_size / 1024));

    // Ekran Başlatma
    init_framebuffer(magic, mbi);
    gfx_init();
    fb_console_init(0x00FFFFFF, 0x00000000);

    // Donanım Sürücüleri
    kbd_init();
    ps2_mouse_init();
    net_init();

    // Zamanlayıcı
    timer_init(1000);
    time_set_tz_offset_sec(3 * 3600);
    time_init_from_rtc();

    // Kesmeleri açmadan önce kritik sistemleri hazırla
    fs_init_once();
    loader_bootstrap(); // Sürücüyü diske enjekte et

    asm volatile("sti");

    // Sürücü Modülünü Yükle
    printk("[Loader] Modul yukleme denemesi basliyor...\n");
    load_module_from_file("/persist/hello.kmod");

    // UI ve Dosya Sistemi Tohumlama
    seed_files_run();
    ui_theme_bootstrap_default();

    // UI Session Başlatma
    ui_session_init();
    ui_session_switch(UI_SESSION_TTY1);

    while (1) {
        // Klavye event dispatch
        uint16_t sc;
        while ((sc = kbd_pop_event()) != 0) {
            ui_session_handle_scancode(sc);
        }

        // Frame tick
        ui_session_tick();

        asm volatile("hlt");
    }
}