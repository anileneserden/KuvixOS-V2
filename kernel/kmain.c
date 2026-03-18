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

#include <ui/session.h>

#include <kernel/fs/fs_init.h>
#include <kernel/fs/ramfs.h>
#include <kernel/fs/kvxfs.h> // KVXFS fonksiyonları için eklendi

#include <kernel/system/seed_files.h>

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
    uint32_t bpp __attribute__((unused)) = 32;

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
    // 1. Temel donanım ve seri port (Logları görmek için en başta olmalı)
    serial_init();
    printk("MB magic=%x\n", magic);

    gdt_init();
    idt_init();

    // 2. Bellek yönetimi (KMALLOC)
    uintptr_t heap_base = align_up((uintptr_t)&_end, 0x1000);
    uint32_t heap_size = 32u * 1024u * 1024u; 
    kmalloc_init((void*)heap_base, heap_size);

    printk("[KMALLOC] _end=%x heap_base=%x heap_size=%x KB\n",
       (uint32_t)(uintptr_t)&_end,
       (uint32_t)heap_base,
       (uint32_t)(heap_size / 1024));

    // 3. Görüntü sürücüleri
    init_framebuffer(magic, mbi);
    gfx_init();
    fb_console_init(0x00FFFFFF, 0x00000000);

    // 4. Giriş aygıtları
    kbd_init();
    ps2_mouse_init();

    // 5. Ağ ve Zamanlayıcı
    net_init();
    timer_init(1000);

    // 6. Dosya Sistemleri (Kritik Bölüm)
    ramfs_init();      
    printk("[FS] RamFS initialized.\n");
    
    fs_init_once();    

    // ARTIK BURADA MANUEL FORMAT ATMA! 
    // Sadece durum kontrolü yap:
    if (kvxfs_init()) {
        printk("[KVXFS] Kalici disk basariyla algilandi.\n");
    } else {
        // Burası formatlamak yerine sadece uyarı versin.
        // Format atmak istersen shell üzerinden bir komutla yaparsın.
        printk("[KVXFS] UYARI: Kalici disk baglanamadi (Formatli olmayabilir).\n");
    }
    seed_files_run();  

    // Interruptları aç
    asm volatile("sti");

    // 7. Kullanıcı Arayüzü (UI)
    ui_session_init();
    ui_session_switch(UI_SESSION_TTY1);

    while (1) {
        // Klavye olaylarını işle
        uint16_t sc;
        while ((sc = kbd_pop_event()) != 0) {
            ui_session_handle_scancode(sc);
        }

        // UI döngüsü
        ui_session_tick();

        asm volatile("hlt");
    }
}