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

#include <kernel/exec/loader.h>

#define DEBUG_TTF_SCREEN 0

extern void gdt_init(void);
extern void idt_init(void);

extern int load_driver_module_named(const char* name, const char* filepath);
typedef int (*driver_read_fn)(void* buffer, uint32_t size);

extern void* getDriverFunction(const char* driver_name, const char* function_name);
extern void session_handle_char(char c);

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
static void init_system_font(void) {
    printk("[TTF] Sistem fontu yukleniyor...\n");

    const char* font_path = "/sys/fonts/arial.ttf";

    vfs_stat_t st;
    if (!vfs_stat(font_path, &st)) {
        printk("[TTF] Hata: '%s' bulunamadi!\n", font_path);
        return;
    }

    if (st.size == 0) {
        printk("[TTF] Hata: '%s' bos!\n", font_path);
        return;
    }

    uint8_t* font_buf = kmalloc(st.size + 16);
    if (!font_buf) {
        printk("[TTF] Hata: Font icin bellek ayrilamadi (%u bayt)\n", st.size);
        return;
    }

    uint32_t bytes_read = 0;
    if (!vfs_read_all(font_path, font_buf, st.size + 16, &bytes_read) || bytes_read == 0) {
        printk("[TTF] Hata: Font dosyasi okunamadi!\n");
        kfree(font_buf);
        return;
    }

    printk("[TTF] Font dosyasi okundu: %u bayt. Baslatiliyor...\n", bytes_read);

    if (!ttf_init_from_memory(font_buf, bytes_read, 16.0f)) {
        printk("[TTF] Hata: ttf_init_from_memory basarisiz oldu!\n");
        kfree(font_buf);
    }
}

#if DEBUG_TTF_SCREEN
static void ttf_debug_draw_string(const char* str, int x, int baseline_y, uint32_t color) {
    if (!ttf_is_initialized() || !str) return;

    int pen_x = x;
    for (int i = 0; str[i] != '\0'; i++) {
        unsigned char ch = (unsigned char)str[i];
        int w = 0, h = 0, xoff = 0, yoff = 0;

        unsigned char* bmp = ttf_get_code_bitmap((uint32_t)ch, &w, &h, &xoff, &yoff);
        if (bmp) {
            for (int row = 0; row < h; row++) {
                for (int col = 0; col < w; col++) {
                    unsigned char alpha = bmp[row * w + col];
                    if (alpha > 45) {
                        fb_putpixel(pen_x + xoff + col, baseline_y + yoff + row, color);
                    }
                }
            }
            ttf_free_bitmap(bmp);
        }

        pen_x += (w > 0 ? w + 2 : 10);
    }
}

static void run_ttf_debug_screen(void) {
    fb_console_set_enabled(false);
    fb_clear(0x0010131A);

    for (int y = 30; y < 74; y++) {
        for (int x = 30; x < 620; x++) {
            fb_putpixel(x, y, 0x001C2230);
        }
    }

    if (ttf_is_initialized()) {
        ttf_debug_draw_string("KuvixOS - TTF Debug Menu", 44, 58, 0x00F5B041);
        ttf_debug_draw_string("Font: /sys/fonts/arial.ttf (OK)", 44, 115, 0x00EAECEF);
        ttf_debug_draw_string("[ESC / F1] Normal acilisa gec", 44, 155, 0x009AA5B4);
    } else {
        gfx_draw_text_utf8(44, 46, 0x00F5B041, "KuvixOS - TTF Debug Menu (Fallback)");
        gfx_draw_text_utf8(44, 96, 0x00FF6B6B, "TTF baslatilamadi (/sys/fonts/arial.ttf kontrol et)");
        gfx_draw_text_utf8(44, 136, 0x009AA5B4, "[ESC / F1] Normal acilisa gec");
    }

    // Karşılaştırma için gfx_draw_text_utf8 (8x16 bitmap + UTF-8/KVX dönüşümü)
    gfx_draw_line(44, 185, 620, 185, 0x002B3344);
    gfx_draw_text_utf8(44, 200, 0x007DD3FC, "[gfx_draw_text_utf8] Karsilastirma Metni (8x16)");
    gfx_draw_text_utf8(44, 224, 0x00EAECEF, "Turkce karakter testi: ç ğ ı İ ö ş ü Ç Ğ Ö Ş Ü");

    fb_present();

    while (1) {
        uint16_t sc;
        while ((sc = kbd_pop_event()) != 0) {
            if (sc == 0x01 || sc == 0x3B) {
                fb_console_set_enabled(true);
                fb_clear(0x00000000);
                fb_present();
                return;
            }
        }
        asm volatile("hlt");
    }
}
#endif

void test_vfs_disk_access() {
    printk("\n[DISK-TEST] VFS disk baglantisi kontrol ediliyor...\n");

    const char* test_file = "/sys/drivers/rtc.kdf";

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

    kbd_init();
    ps2_mouse_init();

    net_init();

    timer_init(1000);
    time_set_tz_offset_sec(3 * 3600);
    time_init_from_rtc();

    asm volatile("sti");

    fs_init_once();

#if DEBUG_TTF_SCREEN
    init_system_font();
    run_ttf_debug_screen();
#else
    init_system_font();
#endif

    test_vfs_disk_access();

    //load_driver_module("/sys/drivers/rtc.kdf");
    load_driver_module_named("ehci", "/sys/drivers/ehci");

    driver_read_fn ehci_read_char = (driver_read_fn)getDriverFunction("ehci", "read_char");

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

        if (ehci_read_char) {
            char ch = 0;
            if (ehci_read_char(&ch, 1) > 0 && ch != 0) {
                session_handle_char(ch);
            }
        }

        shell_tick();

        asm volatile("pause");
    }
}