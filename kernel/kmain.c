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

// ✅ heap/kmalloc
#include <kernel/memory/kmalloc.h>

// UI / Session
#include <ui/session.h>

#include <kernel/fs/fs_init.h>
#include <kernel/fs.h> // VFS fonksiyonları için
#include <kernel/fs/vfs.h>

#include <ui/inputtest.h>
#include <ui/theme.h>

// ✅ KEF Loader
#include <kernel/system/kef_loader.h>

extern void gdt_init(void);
extern void idt_init(void);

// ✅ linker.ld’den geliyor
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

    fb_init(addr, w, h, pitch);
}

// Dosyaları listelemek için yardımcı callback
static int debug_vfs_list_cb(const char* path, uint32_t size, void* u) {
    (void)u;
    printk("  -> %s (%u bytes)\n", path, size);
    return 1;
}

// ------------------------------------------------------------
// Kernel entry
// ------------------------------------------------------------
void kernel_main(uint32_t magic, multiboot_info_t* mbi) {
    serial_init();
    printk("KuvixOS Booting...\n");

    gdt_init();
    idt_init();

    // =========================================================
    // ✅ HEAP INIT
    // =========================================================
    uintptr_t heap_base = align_up((uintptr_t)&_end, 0x1000);
    uint32_t heap_size = 32u * 1024u * 1024u;
    kmalloc_init((void*)heap_base, heap_size);

    printk("[KMALLOC] Heap initialized at %x\n", heap_base);

    // =========================================================
    
    init_framebuffer(magic, mbi);
    gfx_init();
    fb_console_init(0x00FFFFFF, 0x00000000);

    kbd_init();
    ps2_mouse_init();

    timer_init(1000);
    time_set_tz_offset_sec(3 * 3600);
    time_init_from_rtc();

    // Kesmeleri aç
    asm volatile("sti");

    // Dosya sistemini başlat
    fs_init_once();

    // =========================================================
    // ✅ KEF-V3 DEBUG & TEST OPERASYONU
    // =========================================================
    
    // 1. Önce /boot içinde ne var görelim (Debug için)
    printk("[KEF] /boot icerigi listeleniyor:\n");
    vfs_list("/boot", debug_vfs_list_cb, NULL);

    // 2. ToyFS kök dizinini kontrol edelim
    printk("[KEF] /removable icerigi listeleniyor:\n");
    vfs_list("/removable", debug_vfs_list_cb, NULL);

    // Senin VFS yapına göre ToyFS dosyaları genelde /removable altında olur.
    // Eğer Makefile'da ISO/boot/test_app.kef içine koyduysan yol şudur:
    const char* target_path = "/removable/boot/test_app.kef";
    
    printk("[KEF] Uygulama deneniyor: %s\n", target_path);
    
    vfs_file_t* file_ptr = NULL;
    if (vfs_open(target_path, VFS_O_RDONLY, &file_ptr)) {
        
        uint32_t read_size = 0;
        // Dosya boyutunu vfs_stat ile alabiliriz ama şimdilik 8KB ayırıyoruz
        void* kef_buf = kmalloc(8192);
        
        if (vfs_read(file_ptr, kef_buf, 8192, &read_size)) {
            printk("[KEF] Yukleniyor (%u byte)...\n", read_size);
            
            // Loader'ı çağır
            run_kef_v3(kef_buf);
        } else {
            printk("[KEF] Hata: Dosya okunamadi.\n");
        }

        vfs_close(file_ptr);
    } else {
        // Eğer üstteki de olmazsa, sadece kök dizinde aramayı dene:
        printk("[KEF] %s bulunamadi, alternatif deneniyor: /removable/test_app.kef\n", target_path);
        if (vfs_open("/removable/test_app.kef", VFS_O_RDONLY, &file_ptr)) {
             // ... (okuma ve çalıştırma işleminin aynısı burada da yapılabilir) ...
             // Ama listeleme sonucu gelirse yolu net göreceğiz.
        }
    }
    // =========================================================

    ui_theme_bootstrap_default();

    // UI Başlat
    ui_session_init();
    ui_session_switch(UI_SESSION_DESKTOP);

    printk("[System] Desktop ready.\n");

    while (1) {
        uint16_t sc;
        while ((sc = kbd_pop_event()) != 0) {
            ui_session_handle_scancode(sc);
        }
        ui_session_tick();
        asm volatile("hlt");
    }
}