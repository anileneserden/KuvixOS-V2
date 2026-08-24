#include <kernel/drivers/video/de_api.h>
#include <kernel/drivers/video/fb_console.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/fs/vfs.h>
#include <kernel/memory/kmalloc.h>
#include <lib/commands.h>
#include <lib/string.h>
#include <kernel/printk.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/rtc/rtc.h>
#include <kernel/drivers/input/mouse_ps2.h>
#include <kernel/drivers/input/keyboard.h>
#include <kernel/fs/kvxfs.h>

#define KDE_DEFAULT_LOAD_ADDRESS 0x00800000

// --- 32-bit ELF YAPILARI ---

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

// --- KBI PIXEL YAPISI ---

#pragma pack(push, 1)
typedef struct {
    uint8_t b, g, r, a;
} KBIPixel;
#pragma pack(pop)

// --- API SARMALAYICILARI (WRAPPERS) ---

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

static void kernel_get_mouse(de_mouse_state_t* state) {
    if (!state) return;

    extern void ps2_mouse_poll(void); 
    ps2_mouse_poll();
    ps2_mouse_update();

    int max_w = (int)fb_get_width();
    int max_h = (int)fb_get_height();

    if (mouse_x < 0) mouse_x = 0;
    if (mouse_y < 0) mouse_y = 0;
    if (mouse_x >= max_w) mouse_x = max_w - 1;
    if (mouse_y >= max_h) mouse_y = max_h - 1;

    state->x = mouse_x;
    state->y = mouse_y;
    state->left_button   = (g_mouse_buttons & 0x01) ? 1 : 0;
    state->right_button  = (g_mouse_buttons & 0x02) ? 1 : 0;
    state->middle_button = (g_mouse_buttons & 0x04) ? 1 : 0;
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

    if (file_buf[0] != 'K' || file_buf[1] != 'B' || 
        file_buf[2] != 'I' || file_buf[3] != '1') {
        kfree(file_buf);
        return 0;
    }

    uint16_t width = *(uint16_t*)&file_buf[4];
    uint16_t height = *(uint16_t*)&file_buf[6];
    
    KBIPixel* pixels = (KBIPixel*)&file_buf[10];

    int max_w = fb_get_width();
    int max_h = fb_get_height();

    // Normal koordinatlı çizim
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

static void kernel_dmg_union_replace(int x1, int y1, int x2, int y2) {
    int w = x2 - x1;
    int h = y2 - y1;

    if (w <= 0 || h <= 0) return;

    fb_present_rect(x1, y1, w, h);
}

static int kernel_read_file(const char* path, char* buffer, uint32_t max_size) {
    if (!path || !buffer || max_size == 0) return -1;
    uint32_t nread = 0;
    if (vfs_read_all(path, (uint8_t*)buffer, max_size, &nread)) {
        return (int)nread;
    }
    return -1;
}

static int kernel_create_file(const char* path, const char* content, uint32_t size) {
    if (!path) return -1;
    
    // Eğer içerik verilmediyse (boş dosya / touch mantığı) boş bir byte ile oluştur
    const uint8_t* write_data = (const uint8_t*)(content ? content : "");
    uint32_t write_size = (content && size > 0) ? size : 0;
    
    // Doğrudan kvxfs_write_all kullanarak dosyayı diske / dosya sistemine yaz
    if (kvxfs_write_all(path, write_data, write_size)) {
        printk("[KDE KERNEL] Dosya basariyla olusturuldu: '%s' (%d bayt)\n", path, write_size);
        return (int)write_size;
    }
    
    printk("[KDE KERNEL HATA] Dosya olusturulamadi: '%s'\n", path);
    return -1;
}

// --- GÜNCELLENMİŞ DOSYA SAYISI KERNEL SARMALAYICISI ---
static int kernel_get_file_count(const char* path) {
    if (!path) {
        printk("[KDE KERNEL HATA] get_file_count: Yol NULL!\n");
        return 0;
    }
    
    int count = kvxfs_get_file_count(path);
    
    // Host terminaline (Serial) açık ve net şekilde yazdırıyoruz:
    printk("[KDE KERNEL] >>> Sorgulanan Dizin: '%s' | Bulunan Dosya/Klasor Sayisi: %d <<<\n", path, count);
    
    return count;
}

static int kernel_get_file_name_at(const char* path, int index, char* dest_name, int max_len) {
    return kvxfs_get_file_name_at(path, index, dest_name, max_len);
}

static DE_API g_kde_api;

void cmd_kde(int argc, char** argv) {
    const char* filepath = "/sys/de/deneme.kde";
    if (argc > 1 && argv[1] != NULL && argv[1][0] != '\0') {
        filepath = argv[1];
    }

    printk("[KDE LOADER] %s yukleniyor...\n", filepath);

    uint32_t max_size = 64 * 1024;
    uint8_t* file_buf = (uint8_t*)kmalloc(max_size);

    if (!file_buf) {
        printk("Hata: Bellek ayrilamadi.\n");
        return;
    }

    memset(file_buf, 0, max_size);

    uint32_t nread = 0;
    if (!vfs_read_all(filepath, file_buf, max_size, &nread) || nread == 0) {
        printk("Hata: %s okunamadi!\n", filepath);
        kfree(file_buf);
        return;
    }

    printk("[KDE LOADER] %d bayt okundu.\n", nread);

    uint32_t entry_point = 0;

    if (nread >= sizeof(elf32_ehdr_t) && 
        file_buf[0] == 0x7F && file_buf[1] == 'E' && 
        file_buf[2] == 'L' && file_buf[3] == 'F') {

        elf32_ehdr_t* ehdr = (elf32_ehdr_t*)file_buf;
        entry_point = ehdr->e_entry;

        printk("[KDE LOADER] ELF tespit edildi. e_entry: 0x%x, phnum: %d\n", entry_point, ehdr->e_phnum);

        elf32_phdr_t* phdr = (elf32_phdr_t*)(file_buf + ehdr->e_phoff);
        for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
            if (phdr[i].p_type == PT_LOAD) {
                void* dest = (void*)(uintptr_t)phdr[i].p_vaddr;
                
                printk("[KDE LOADER] Segment %d -> vaddr: 0x%x, offset: 0x%x, filesz: %d\n", 
                       i, phdr[i].p_vaddr, phdr[i].p_offset, phdr[i].p_filesz);

                memcpy(dest, file_buf + phdr[i].p_offset, phdr[i].p_filesz);

                if (phdr[i].p_memsz > phdr[i].p_filesz) {
                    memset((uint8_t*)dest + phdr[i].p_filesz, 0, phdr[i].p_memsz - phdr[i].p_filesz);
                }
            }
        }

    } else {
        entry_point = KDE_DEFAULT_LOAD_ADDRESS;
        printk("[KDE LOADER] Raw Binary tespit edildi. 0x%x adresine kopyalaniyor...\n", entry_point);
        memcpy((void*)(uintptr_t)entry_point, file_buf, nread);
    }

    kfree(file_buf);

    memset(&g_kde_api, 0, sizeof(DE_API));
    g_kde_api.screen_width      = fb_get_width();
    g_kde_api.screen_height     = fb_get_height();
    g_kde_api.put_pixel         = fb_putpixel;
    g_kde_api.draw_rect         = kernel_draw_rect;
    g_kde_api.draw_text         = kernel_draw_text;
    g_kde_api.clear_screen      = fb_clear;
    g_kde_api.update_display    = fb_present;
    g_kde_api.get_mouse         = kernel_get_mouse;
    g_kde_api.get_key           = kernel_get_key;
    g_kde_api.get_time          = kernel_get_time;
    g_kde_api.log               = kernel_log;
    g_kde_api.render_kbi        = kernel_render_kbi;
    g_kde_api.dmg_union_replace = kernel_dmg_union_replace;
    g_kde_api.create_file       = kernel_create_file;
    g_kde_api.read_file         = kernel_read_file;
    g_kde_api.fill_round_rect   = gfx_fill_round_rect;
    g_kde_api.get_file_count    = kernel_get_file_count;
    g_kde_api.get_file_name_at  = kernel_get_file_name_at;
    g_kde_api.ksprintf = ksprintf;
    g_kde_api.strlen  = strlen;
    g_kde_api.strcmp  = strcmp;
    g_kde_api.strncmp = strncmp;
    g_kde_api.strrchr = strrchr;

    printk("[KDE LOADER] Giriş noktasına atlaniyor: 0x%x\n", entry_point);

    fb_console_set_enabled(false);
    fb_clear(0x000000);
    fb_present();

    typedef void (__attribute__((cdecl)) *kde_entry_t)(DE_API*);
    kde_entry_t start_desktop = (kde_entry_t)(uintptr_t)entry_point;

    start_desktop(&g_kde_api);

    fb_console_set_enabled(true);
    printk("[KDE LOADER] Masaustu kapandi.\n");
}

REGISTER_COMMAND(kde, cmd_kde, "Starts KuvixOS DEDK V2 Desktop Environment");