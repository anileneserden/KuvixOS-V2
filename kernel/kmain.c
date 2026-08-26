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

#include <init/init.h>
#include <init/session.h>

#include <kernel/fs/fs_init.h>

#include <kernel/system/seed_files.h>

#include <kernel/fs/vfs.h>

#include <kernel/user.h>
#include <lib/shell.h>

#include <kernel/drivers/video/ttf.h>

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
    (void)bpp;

    if (magic == 0x2BADB002 && mbi && (mbi->flags & (1 << 12))) {
        addr  = (uint32_t)mbi->framebuffer_addr;
        w     = (uint32_t)mbi->framebuffer_width;
        h     = (uint32_t)mbi->framebuffer_height;
        pitch = (uint32_t)mbi->framebuffer_pitch;
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
// TTF Font Loader init
// ------------------------------------------------------------
// ------------------------------------------------------------
// TTF Font Loader init
// ------------------------------------------------------------
static void init_system_font(void) {
    printk("[TTF] Sistem fontu yukleniyor...\n");
    
    uint32_t max_size = 256 * 1024; // 256 KB font alanı
    uint8_t* font_buf = kmalloc(max_size);
    if (!font_buf) {
        printk("[TTF] Hata: Font icin bellek ayrilamadi!\n");
        return;
    }

    const char* font_path = "/sys/fonts/arial.ttf";
    
    vfs_stat_t st;
    if (!vfs_stat(font_path, &st)) {
        printk("[TTF] Hata: '%s' bulunamadi!\n", font_path);
        kfree(font_buf);
        return;
    }

    vfs_file_t* file = NULL;
    if (!vfs_open(font_path, 0, &file)) {
        printk("[TTF] Hata: '%s' acilamadi!\n", font_path);
        kfree(font_buf);
        return;
    }

    uint32_t bytes_read = 0;
    int read_res = vfs_read(file, font_buf, max_size, &bytes_read);
    
    // Eğer vfs_close fonksiyonun varsa burada dosyayı kapatabilirsin
    // vfs_close(file);

    if (!read_res || bytes_read == 0) {
        printk("[TTF] Hata: Font dosyasi okunamadi!\n");
        kfree(font_buf);
        return;
    }

    printk("[TTF] Font dosyasi okundu: %u bayt. Baslatiliyor...\n", bytes_read);

    // ✅ Doğru çağrı: Bellek, boyut ve piksel yüksekliği (16.0f) veriliyor
    if (!ttf_init_from_memory(font_buf, bytes_read, 16.0f)) {
        printk("[TTF] Hata: ttf_init_from_memory basarisiz oldu!\n");
        kfree(font_buf);
    }
}

void test_vfs_disk_access() {
    printk("\n[DISK-TEST] VFS disk baglantisi kontrol ediliyor...\n");
    
    const char* test_file = "/home/anil/mouse_ps2.kdf"; 
    
    vfs_stat_t st;
    if (vfs_stat(test_file, &st)) {
        printk("[DISK-TEST] OK: '%s' bulundu! Backend: %d\n", test_file, st.backend);
        
        if (st.backend == 3) {
            printk("[DISK-TEST] OK: Backend 3 (KVXFS) dogrulandi.\n");
        } else {
            printk("[DISK-TEST] UYARI: Dosya bulundu ama backend beklenenden farkli: %d\n", st.backend);
        }
    } else {
        printk("[DISK-TEST] HATA: '%s' bulunamadi! VFS yolu veya mount hatasi.\n", test_file);
    }
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

    // 🚀 TTF Font Yükleyicisini Başlat
    init_system_font();

    kbd_init();
    ps2_mouse_init();

    net_init();

    timer_init(1000);
    time_set_tz_offset_sec(3 * 3600);
    time_init_from_rtc();

    asm volatile("sti");

    fs_init_once();

    test_vfs_disk_access();

    vfs_set_cwd("/home/anil");

    shell_set_username("anil");
    shell_set_hostname("kuvix");

    os_init();

    session_init();

    while (1) {
        uint16_t sc;
        while ((sc = kbd_pop_event()) != 0) {
            session_handle_scancode(sc);
        }

        shell_tick();

        asm volatile("hlt");
    }
}