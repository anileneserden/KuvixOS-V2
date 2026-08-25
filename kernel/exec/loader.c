#include <kernel/drivers/video/de_api.h>
#include <kernel/drivers/video/login_api.h>
#include <kernel/drivers/video/fb_console.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/fs/vfs.h>
#include <kernel/memory/kmalloc.h>
#include <lib/string.h>
#include <kernel/printk.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/rtc/rtc.h>
#include <kernel/drivers/input/mouse_ps2.h>
#include <kernel/drivers/input/keyboard.h>
#include <kernel/fs/kvxfs.h>

#define DEFAULT_LOAD_ADDRESS 0x00800000

// --- ORTAK ELF YAPILARI VE KBI ---
typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} __attribute__((packed)) elf32_ehdr_t;

typedef struct {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
} __attribute__((packed)) elf32_phdr_t;

#define PT_LOAD 1

#pragma pack(push, 1)
typedef struct {
    uint8_t b, g, r, a;
} KBIPixel;
#pragma pack(pop)

// İleri bildirimler (Forward Declarations)
void load_desktop_module(const char* filepath);
void load_login_module(const char* filepath);

// --- ORTAK KERNEL WRAPPER'LARI ---
static void kernel_draw_rect(int x, int y, int w, int h, uint32_t color) {
    int max_w = fb_get_width();
    int max_h = fb_get_height();
    int end_x = x + w;
    int end_y = y + h;

    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (end_x > max_w) end_x = max_w;
    if (end_y > max_h) end_y = max_h;

    for (int cy = y; cy < end_y; cy++) {
        for (int cx = x; cx < end_x; cx++) {
            fb_putpixel(cx, cy, color);
        }
    }
}

static void kernel_draw_text(int x, int y, const char* text, uint32_t color) {
    if (!text || text[0] == '\0') return;
    gfx_draw_text_utf8(x, y, color, text);
}

static char kernel_get_key(void) {
    kbd_poll();
    if (kbd_has_character()) {
        return kbd_get_char();
    }
    return 0;
}

static void kernel_get_time(char* buffer) {
    if (!buffer) return;
    rtc_datetime_t dt;
    if (rtc_read_datetime(&dt)) {
        buffer[0] = (dt.hour / 10) + '0'; buffer[1] = (dt.hour % 10) + '0';
        buffer[2] = ':';
        buffer[3] = (dt.min / 10) + '0';  buffer[4] = (dt.min % 10) + '0';
        buffer[5] = ':';
        buffer[6] = (dt.sec / 10) + '0';  buffer[7] = (dt.sec % 10) + '0';
        buffer[8] = '\0';
    } else {
        strcpy(buffer, "00:00:00");
    }
}

static void kernel_log(const char* msg) {
    if (msg) printk("%s", msg);
}

static int kernel_render_kbi(int target_x, int target_y, const char* filepath) {
    if (!filepath) return 0;
    uint32_t max_size = 10 * 1024 * 1024; 
    uint8_t* file_buf = (uint8_t*)kmalloc(max_size);
    if (!file_buf) return 0;

    uint32_t nread = 0;
    if (!vfs_read_all(filepath, file_buf, max_size, &nread) || nread < 10) {
        kfree(file_buf);
        return 0;
    }

    if (file_buf[0] != 'K' || file_buf[1] != 'B' || file_buf[2] != 'I' || file_buf[3] != '1') {
        kfree(file_buf);
        return 0;
    }

    uint16_t width = *(uint16_t*)&file_buf[4];
    uint16_t height = *(uint16_t*)&file_buf[6];
    KBIPixel* pixels = (KBIPixel*)&file_buf[10];

    int max_w = fb_get_width();
    int max_h = fb_get_height();

    if (target_x == 0 && target_y == 0 && width == max_w && height == max_h) {
        uint32_t* fb = fb_backbuffer_ptr();
        if (fb) {
            for (uint32_t i = 0; i < (uint32_t)(width * height); i++) {
                KBIPixel p = pixels[i];
                fb[i] = ((uint32_t)p.r << 16) | ((uint32_t)p.g << 8) | p.b;
            }
            kfree(file_buf);
            return 1;
        }
    }

    for (uint16_t y = 0; y < height; y++) {
        int py = target_y + y;
        if (py < 0 || py >= max_h) continue;

        for (uint16_t x = 0; x < width; x++) {
            int px = target_x + x;
            if (px < 0 || px >= max_w) continue;

            KBIPixel p = pixels[y * width + x];
            if (p.a > 0) {
                uint32_t color = ((uint32_t)p.r << 16) | ((uint32_t)p.g << 8) | p.b;
                fb_putpixel(px, py, color);
            }
        }
    }

    kfree(file_buf);
    return 1;
}

static int kernel_read_file(const char* path, char* buffer, uint32_t max_size) {
    if (!path || !buffer || max_size == 0) return -1;
    uint32_t nread = 0;
    if (vfs_read_all(path, (uint8_t*)buffer, max_size, &nread)) {
        return (int)nread;
    }
    return -1;
}

// --- MOUSE WRAPPERS ---
static void kernel_get_de_mouse(de_mouse_state_t* state) {
    if (!state) return;
    extern void ps2_mouse_poll(void); 
    ps2_mouse_poll();
    ps2_mouse_update();
    state->x = mouse_x; state->y = mouse_y;
    state->left_button   = (g_mouse_buttons & 0x01) ? 1 : 0;
    state->right_button  = (g_mouse_buttons & 0x02) ? 1 : 0;
    state->middle_button = (g_mouse_buttons & 0x04) ? 1 : 0;
}

static void kernel_get_login_mouse(login_mouse_state_t* state) {
    if (!state) return;
    extern void ps2_mouse_poll(void); 
    ps2_mouse_poll();
    ps2_mouse_update();
    state->x = mouse_x; state->y = mouse_y;
    state->left_button   = (g_mouse_buttons & 0x01) ? 1 : 0;
    state->right_button  = (g_mouse_buttons & 0x02) ? 1 : 0;
    state->middle_button = (g_mouse_buttons & 0x04) ? 1 : 0;
}

// --- ORTAK ELF OKUYUCU YARDIMCISI ---
static uint32_t load_elf_or_binary(const char* filepath) {
    uint32_t max_size = 64 * 1024;
    uint8_t* file_buf = (uint8_t*)kmalloc(max_size);
    if (!file_buf) return 0;

    memset(file_buf, 0, max_size);
    uint32_t nread = 0;
    if (!vfs_read_all(filepath, file_buf, max_size, &nread) || nread == 0) {
        kfree(file_buf);
        return 0;
    }

    uint32_t entry_point = 0;
    if (nread >= sizeof(elf32_ehdr_t) && 
        file_buf[0] == 0x7F && file_buf[1] == 'E' && 
        file_buf[2] == 'L' && file_buf[3] == 'F') {

        elf32_ehdr_t* ehdr = (elf32_ehdr_t*)file_buf;
        entry_point = ehdr->e_entry;

        elf32_phdr_t* phdr = (elf32_phdr_t*)(file_buf + ehdr->e_phoff);
        for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
            if (phdr[i].p_type == PT_LOAD) {
                void* dest = (void*)(uintptr_t)phdr[i].p_vaddr;
                memcpy(dest, file_buf + phdr[i].p_offset, phdr[i].p_filesz);
                if (phdr[i].p_memsz > phdr[i].p_filesz) {
                    memset((uint8_t*)dest + phdr[i].p_filesz, 0, phdr[i].p_memsz - phdr[i].p_filesz);
                }
            }
        }
    } else {
        entry_point = DEFAULT_LOAD_ADDRESS;
        memcpy((void*)(uintptr_t)entry_point, file_buf, nread);
    }

    kfree(file_buf);
    return entry_point;
}

// --- LOGIN BAŞARILI OLDUĞUNDA ÇALIŞACAK FONKSİYON ---
static void kernel_start_desktop_from_login(void) {
    printk("[LOGIN API] Giris basarili, masaustune geciliyor...\n");
    
    char cfg_buf[256];
    const char* target_desktop = "/sys/de/desktopIconsLoad.kde"; 

    int bytes = kernel_read_file("/sys/configs/session.cfg", cfg_buf, sizeof(cfg_buf) - 1);
    if (bytes > 0) {
        cfg_buf[bytes] = '\0';
        char* line = cfg_buf;
        while (*line != '\0') {
            while (*line == ' ' || *line == '\t' || *line == '\r') line++;

            if (strncmp(line, "desktopScreen", 13) == 0) {
                char* p = line + 13;
                while (*p == ' ' || *p == '\t' || *p == '=') p++;

                char* val_start = p;
                while (*p != '\0' && *p != '\n' && *p != '\r' && *p != ' ' && *p != '\t') {
                    p++;
                }
                
                if (p > val_start) {
                    static char parsed_path[128];
                    int len = p - val_start;
                    if (len >= (int)sizeof(parsed_path)) len = (int)sizeof(parsed_path) - 1;
                    
                    memcpy(parsed_path, val_start, len);
                    parsed_path[len] = '\0';

                    target_desktop = parsed_path;
                    break;
                }
            }
            while (*line != '\0' && *line != '\n') line++;
            if (*line == '\n') line++;
        }
    }

    // Login modülünün döngüsünden çıkıp masaüstünü başlat
    load_desktop_module(target_desktop);
}

// --- 1. MASAÜSTÜ YÜKLEYİCİ (.kde) ---
static DE_API g_de_api;

void load_desktop_module(const char* filepath) {
    if (!filepath) return;
    printk("[LOADER] Desktop yukleniyor: %s\n", filepath);

    uint32_t entry_point = load_elf_or_binary(filepath);
    if (!entry_point) {
        printk("Hata: Desktop yuklenemedi!\n");
        return;
    }

    memset(&g_de_api, 0, sizeof(DE_API));
    g_de_api.screen_width      = fb_get_width();
    g_de_api.screen_height     = fb_get_height();
    g_de_api.put_pixel         = fb_putpixel;
    g_de_api.draw_rect         = kernel_draw_rect;
    g_de_api.draw_text         = kernel_draw_text;
    g_de_api.clear_screen      = fb_clear;
    g_de_api.update_display    = fb_present;
    g_de_api.get_mouse         = kernel_get_de_mouse;
    g_de_api.get_key           = kernel_get_key;
    g_de_api.get_time          = kernel_get_time;
    g_de_api.log               = kernel_log;
    g_de_api.render_kbi        = kernel_render_kbi;
    g_de_api.dmg_union_replace = (void*)(uintptr_t)fb_present_rect;
    g_de_api.create_file       = (void*)(uintptr_t)kvxfs_write_all;
    g_de_api.read_file         = kernel_read_file;
    g_de_api.fill_round_rect   = gfx_fill_round_rect;
    g_de_api.fill_round_rect4  = gfx_fill_round_rect4;
    g_de_api.get_file_count    = kvxfs_get_file_count;
    g_de_api.get_file_name_at  = kvxfs_get_file_name_at;

    fb_console_set_enabled(false);
    fb_clear(0x000000);
    fb_present();

    typedef void (__attribute__((cdecl)) *de_entry_t)(DE_API*);
    de_entry_t start_de = (de_entry_t)(uintptr_t)entry_point;
    start_de(&g_de_api);

    fb_console_set_enabled(true);
    printk("[LOADER] Desktop sonlandirildi.\n");
}

// --- 2. GİRİŞ EKRANI YÜKLEYİCİ (.kls) ---
static LoginAPI g_login_api;

void load_login_module(const char* filepath) {
    if (!filepath) return;
    printk("[LOADER] Login ekrani yukleniyor: %s\n", filepath);

    uint32_t entry_point = load_elf_or_binary(filepath);
    if (!entry_point) {
        printk("Hata: Login ekrani yuklenemedi!\n");
        return;
    }

    memset(&g_login_api, 0, sizeof(LoginAPI));
    g_login_api.screen_width   = fb_get_width();
    g_login_api.screen_height  = fb_get_height();
    g_login_api.put_pixel      = fb_putpixel;
    g_login_api.draw_rect      = kernel_draw_rect;
    g_login_api.draw_text      = kernel_draw_text;
    g_login_api.clear_screen   = fb_clear;
    g_login_api.update_display = fb_present;
    g_login_api.get_mouse      = kernel_get_login_mouse;
    g_login_api.get_key        = kernel_get_key;
    g_login_api.get_time       = kernel_get_time;
    g_login_api.log            = kernel_log;
    g_login_api.read_file      = kernel_read_file;
    g_login_api.render_kbi     = kernel_render_kbi;
    g_login_api.start_desktop  = kernel_start_desktop_from_login; // Yeni bağlama!

    fb_console_set_enabled(false);
    fb_clear(0x000000);
    fb_present();

    typedef void (__attribute__((cdecl)) *login_entry_t)(LoginAPI*);
    login_entry_t start_login = (login_entry_t)(uintptr_t)entry_point;
    start_login(&g_login_api);

    fb_console_set_enabled(true);
    printk("[LOADER] Login ekrani sonlandirildi.\n");
}

// Geriye dönük uyumluluk veya eski çağrılar için:
void load_user_module(const char* filepath) {
    load_desktop_module(filepath);
}