// ui/desktop.c
#include <ui/desktop.h>
#include <ui/desktop_icons.h>
#include <ui/dialogs/messagebox.h>
#include <ui/wm.h>
#include <ui/cursor.h>
#include <app/app_manager.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/input/mouse_ps2.h>
#include <kernel/drivers/input/keyboard.h>
#include <lib/math.h>
#include <lib/string.h>
#include <ui/notification.h>
#include <ui/topbar.h>
#include <ui/context_menu.h>
#include <kernel/fs/vfs.h>
#include <ui/apps/notepad.h>
#include <ui/apps/kuvix_browser.h>

#include <ui/window/window.h>

#include <kernel/drivers/ata_pio.h>
#include <kernel/block/block.h>
#include <kernel/printk.h>

#include <ui/dialogs/save_dialog.h>
#include <ui/dialogs/open_dialog.h>

#include <kernel/user.h>

#include <stdbool.h>
#include <stdint.h>

#include <kernel/serial.h>

#include <ui/ui_settings.h>

#include <ui/desktop_seed.h>

#include <ui/apps/memmon.h>

#include <kernel/system/removable.h>

#include <kernel/memory/kmalloc.h>

#include <ui/desktop_icons/folder_icon.h>

// --- DIŞ BİLDİRİMLER ---
extern char kbd_scancode_to_ascii(uint8_t scancode);
extern void desktop_icons_handle_key(uint16_t scancode, char ascii);
extern void desktop_icons_begin_edit(int index);
extern bool desktop_icons_is_any_editing(void);

// ticks (double click için)
extern uint32_t g_ticks_ms;

#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif
#ifndef abs
#define abs(a) ((a) < 0 ? -(a) : (a))
#endif

// ============================================================
// Present helpers (dirty rect)
// ============================================================

static inline void present_rect_safe(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return;
    
    int W = (int)fb_get_width();
    int H = (int)fb_get_height();

    // Sınır kontrolleri
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x >= W || y >= H) return;
    if (x + w > W) w = W - x;
    if (y + h > H) h = H - y;

    if (w <= 0 || h <= 0) return;
    fb_present_rect(x, y, w, h);
}

// ============================================================
// Desktop State (DOSYA SCOPE STATIC)
// ============================================================

static uint32_t desktop_bg_color = 0xFF182838;

uint32_t desktop_get_bg_color(void) {
    return ui_get_desktop_bg();
}

void desktop_set_bg_color(uint32_t argb) {
    ui_set_desktop_bg(argb);
    desktop_invalidate_full();
}

static bool is_selecting = false;
static int  sel_start_x = 0;
static int  sel_start_y = 0;

static int rename_target_index = -1;
static int g_ctx_icon_index = -1;

// mouse state
static uint8_t g_last_btn = 0;

// drag + double click state
static int g_lmb_down = 0;
static int g_down_x = 0;
static int g_down_y = 0;
static int g_down_hit = -1;
static int g_dragging = 0;

static int g_dbg_last_dx = 0;
static int g_dbg_last_dy = 0;
static int g_dbg_wheel_step = 0;
static int g_dbg_wheel_total = 0;

static bool g_need_redraw = true;

static int g_dbg_redraw_reason = 0;

static bool g_dbg_visualize_dirty = false;

static uint32_t last_click_time = 0;
static int last_hit_index = -1;

void desktop_request_redraw(void) {
    g_need_redraw = true;
}

// ============================================================
// Damage (dirty rect) tracking (WM -> Desktop)
// ============================================================
static int g_dmg_valid = 0;
static int g_dmg_x = 0, g_dmg_y = 0, g_dmg_w = 0, g_dmg_h = 0;

static inline void dmg_union_inplace(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) return;

    if (!g_dmg_valid) {
        g_dmg_valid = 1;
        g_dmg_x = x; g_dmg_y = y; g_dmg_w = w; g_dmg_h = h;
        return;
    }

    int x2  = x + w;
    int y2  = y + h;
    int ax2 = g_dmg_x + g_dmg_w;
    int ay2 = g_dmg_y + g_dmg_h;

    int nx1 = (x < g_dmg_x) ? x : g_dmg_x;
    int ny1 = (y < g_dmg_y) ? y : g_dmg_y;
    int nx2 = (x2 > ax2) ? x2 : ax2;
    int ny2 = (y2 > ay2) ? y2 : ay2;

    g_dmg_x = nx1;
    g_dmg_y = ny1;
    g_dmg_w = nx2 - nx1;
    g_dmg_h = ny2 - ny1;
}

void desktop_damage_rect(int x, int y, int w, int h) {
    // 1. Önce alanı hesapla
    dmg_union_inplace(x, y, w, h);
    
    // 2. Sisteme "Artık geçerli bir hasarlı alan var, çizim yap" de
    g_dmg_valid = 1; 
    
    // 3. Redraw isteğini tetikle
    desktop_request_redraw();
}

int desktop_consume_damage_rect(int* x, int* y, int* w, int* h) {
    if (!g_dmg_valid) return 0;

    if (x) *x = g_dmg_x;
    if (y) *y = g_dmg_y;
    if (w) *w = g_dmg_w;
    if (h) *h = g_dmg_h;

    g_dmg_valid = 0;
    return 1;
}

// --- Debug overlay helps (desktop içi) ---
static bool g_dbg_overlay = false;

static void dbg_itoa(int v, char* out) {
    char tmp[16];
    int i = 0, neg = 0;
    if (v < 0) { neg = 1; v = -v; }
    if (v == 0) tmp[i++] = '0';
    while (v > 0 && i < 15) { tmp[i++] = (char)('0' + (v % 10)); v /= 10; }
    int p = 0;
    if (neg) out[p++] = '-';
    while (i > 0) out[p++] = tmp[--i];
    out[p] = 0;
}

static void dbg_draw_kv(int x, int y, const char* k, int v) {
    char n[16];
    dbg_itoa(v, n);
    gfx_draw_text_utf8(x, y, 0x00FFFFFF, k);
    gfx_draw_text_utf8(x + 92, y, 0x00FFFF00, n);
}

static void dbg_draw_panel(void) {
    if (!g_dbg_overlay) return;

    int x = 8, y = 8;
    int w = 220, h = 112;

    gfx_fill_rect(x - 4, y - 4, w, h, 0x00202020);
    gfx_draw_rect(x - 4, y - 4, w, h, 0x00AAAAAA);

    gfx_draw_text_utf8(x, y, 0x00FFFFFF, "DEBUG INPUT (F12 toggle)");
    y += 16;

    dbg_draw_kv(x, y, "dx", g_dbg_last_dx); y += 14;
    dbg_draw_kv(x, y, "dy", g_dbg_last_dy); y += 14;
    dbg_draw_kv(x, y, "wheel", g_dbg_wheel_step); y += 14;
    dbg_draw_kv(x, y, "w_total", g_dbg_wheel_total); y += 14;
    dbg_draw_kv(x, y, "btn", (int)g_last_btn);
}

static uint32_t g_last_click_ms = 0;
static int      g_last_click_hit = -1;

static const int      DRAG_THRESHOLD_PX = 6;
static const uint32_t DBLCLICK_MS = 350;

static bool g_open_after_rename = false;
static char g_open_after_rename_path[256];

static bool g_force_full_present = false;

static void desktop_toggle_ext(void);

void desktop_invalidate_full(void) {
    g_force_full_present = true;
    g_need_redraw = true;
}

#define CURW 24
#define CURH 24
#define CURPAD 10  // present padding (shadow/AA varsa arttır)

static int s_cur_prev_x = -1;
static int s_cur_prev_y = -1;
static uint32_t s_cur_under[CURW * CURH];

static inline void cursor_restore_under(int x, int y) {
    uint32_t* bb = fb_backbuffer_ptr();
    if (!bb) return;

    int W = (int)fb_get_width();
    int H = (int)fb_get_height();

    for (int row = 0; row < CURH; row++) {
        int yy = y + row;
        if ((unsigned)yy >= (unsigned)H) continue;

        uint32_t* dst = bb + yy * W;
        const uint32_t* src = s_cur_under + row * CURW;

        for (int col = 0; col < CURW; col++) {
            int xx = x + col;
            if ((unsigned)xx >= (unsigned)W) continue;
            dst[xx] = src[col];
        }
    }
}

static inline void cursor_save_under(int x, int y) {
    uint32_t* bb = fb_backbuffer_ptr();
    if (!bb) return;

    int W = (int)fb_get_width();
    int H = (int)fb_get_height();

    for (int row = 0; row < CURH; row++) {
        int yy = y + row;
        uint32_t* dst = s_cur_under + row * CURW;

        for (int col = 0; col < CURW; col++) {
            int xx = x + col;
            uint32_t v = 0;
            if ((unsigned)xx < (unsigned)W && (unsigned)yy < (unsigned)H) {
                v = bb[yy * W + xx];
            }
            dst[col] = v;
        }
    }
}

// scene_redrawn=true => sahne yeni çizildi, önce under save sonra cursor çiz
static inline void cursor_overlay_step(int x, int y, bool scene_redrawn) {
    if (scene_redrawn) {
        // Sahne baştan çizildi: backbuffer’da eski cursor zaten yok.
        // Ama frontbuffer’da eski cursor kalmış olabilir -> eski rect’i present ederek temizle.
        if (s_cur_prev_x >= 0) {
            present_rect_safe(s_cur_prev_x - CURPAD, s_cur_prev_y - CURPAD,
                            CURW + CURPAD * 2, CURH + CURPAD * 2);
        }

        // Yeni cursor’u backbuffer’a çiz
        cursor_save_under(x, y);
        cursor_draw_arrow(x, y);

        // Yeni cursor’u da present et
        present_rect_safe(x - CURPAD, y - CURPAD,
                        CURW + CURPAD * 2, CURH + CURPAD * 2);

        s_cur_prev_x = x;
        s_cur_prev_y = y;
        return;
    }

    if (s_cur_prev_x < 0) {
        cursor_save_under(x, y);
        cursor_draw_arrow(x, y);
        present_rect_safe(x - CURPAD, y - CURPAD, CURW + CURPAD * 2, CURH + CURPAD * 2);
        s_cur_prev_x = x;
        s_cur_prev_y = y;
        return;
    }

    // old restore
    cursor_restore_under(s_cur_prev_x, s_cur_prev_y);

    // new save + draw
    cursor_save_under(x, y);
    cursor_draw_arrow(x, y);

    // present old+new
    present_rect_safe(s_cur_prev_x - CURPAD, s_cur_prev_y - CURPAD, CURW + CURPAD * 2, CURH + CURPAD * 2);
    present_rect_safe(x          - CURPAD, y          - CURPAD, CURW + CURPAD * 2, CURH + CURPAD * 2);

    s_cur_prev_x = x;
    s_cur_prev_y = y;
}

// sahne full redraw olduysa (fb_clear + wm_draw vs) cursor overlay state resetlemek için
static inline void cursor_overlay_reset(void) {
    s_cur_prev_x = -1;
    s_cur_prev_y = -1;
}

static void desktop_draw_selection_rect(void) {
    if (!is_selecting) return;

    int rx = min(sel_start_x, mouse_x);
    int ry = min(sel_start_y, mouse_y);
    int rw = abs(mouse_x - sel_start_x) + 1;
    int rh = abs(mouse_y - sel_start_y) + 1;

    // Şeffaf mavi dolgu
    gfx_draw_alpha_rect(rw, rh, 0, 85, 170, 150, rx, ry);
    // İnce kenarlık (isteğe bağlı, daha belirgin yapar)
    gfx_draw_rect(rx, ry, rw, rh, 0x00AAFF);
}

// ============================================================
// Helpers
// ============================================================

static void simple_itoa(int n, char* s) {
    int i, sign;
    if ((sign = n) < 0) n = -n;
    i = 0;
    do { s[i++] = n % 10 + '0'; } while ((n /= 10) > 0);
    if (sign < 0) s[i++] = '-';
    s[i] = '\0';
    for (int j = 0, k = i - 1; j < k; j++, k--) {
        char t = s[j]; s[j] = s[k]; s[k] = t;
    }
}

static bool file_exists(const char* path) {
    vfs_file_t* f = 0;
    if (vfs_open(path, VFS_O_RDONLY, &f) == 1) {
        vfs_close(f);
        return true;
    }
    return false;
}

static void get_unique_filename(const char* base_path, const char* ext, char* out_path) {
    char temp_path[256];
    char num_str[16];
    int counter = 0;

    strcpy(temp_path, base_path);
    strcat(temp_path, ext);

    while (file_exists(temp_path)) {
        counter++;
        simple_itoa(counter, num_str);

        strcpy(temp_path, base_path);
        strcat(temp_path, "_");
        strcat(temp_path, num_str);
        strcat(temp_path, ext);
    }

    strcpy(out_path, temp_path);
}

static void desktop_toggle_ext(void) {
    ui_toggle_show_extensions();
    desktop_icons_init();
    desktop_icons_snap_all();
    desktop_invalidate_full();
}

void seed_store_repo(void) {
    vfs_mkdir("/system");
    vfs_mkdir("/system/repo");
    vfs_mkdir("/system/repo/apps");

    const char* notepad =
        "title=Notepad\n"
        "app_id=3\n"
        "desc=Basit metin editoru\n"
        "icon=/system/icons/notepad.kbi\n";

    vfs_write_all("/system/repo/apps/notepad.kapp",
                  (const uint8_t*)notepad,
                  strlen(notepad));

    const char* terminal =
        "title=Terminal\n"
        "app_id=1\n"
        "desc=Komut satiri\n"
        "icon=/system/icons/terminal.kbi\n";

    vfs_write_all("/system/repo/apps/terminal.kapp",
                  (const uint8_t*)terminal,
                  strlen(terminal));
}

// basit KBI writer (ARGB8888)
static void kbi_write_demo_terminal_icon(void) {
    vfs_mkdir("/system");
    vfs_mkdir("/system/icons");

    const char* path = "/system/icons/terminal.kbi";

    // varsa tekrar yazmak istemiyorsan stat check koyabilirsin
    vfs_stat_t st;
    if (vfs_stat(path, &st)) return; // zaten var

    const int W = 32, H = 32;
    const uint32_t MAGIC = 0x3149424B; // 'KBI1'
    const uint32_t FLAGS = 1; // ARGB8888

    // header + pixels
    uint32_t total = (uint32_t)(sizeof(uint32_t)*1 + sizeof(uint16_t)*2 + sizeof(uint32_t)*2) + (uint32_t)(W*H*4);
    // daha temiz: struct kullan
    typedef struct __attribute__((packed)) {
        uint32_t magic;
        uint16_t w;
        uint16_t h;
        uint32_t flags;
        uint32_t reserved;
    } hdr_t;

    uint32_t cap = (uint32_t)sizeof(hdr_t) + (uint32_t)(W*H*4);
    uint8_t* buf = (uint8_t*)kmalloc(cap);
    if (!buf) return;

    hdr_t hdr;
    hdr.magic = MAGIC;
    hdr.w = (uint16_t)W;
    hdr.h = (uint16_t)H;
    hdr.flags = FLAGS;
    hdr.reserved = 0;

    memcpy(buf, &hdr, sizeof(hdr));

    uint32_t* px = (uint32_t*)(buf + sizeof(hdr));
    // default transparent
    for (int i = 0; i < W*H; i++) px[i] = 0x00000000;

    // basit ikon: siyah çerçeve + turuncu terminal ekranı
    for (int y = 2; y < H-2; y++) {
        for (int x = 2; x < W-2; x++) {
            // frame
            if (x == 2 || y == 2 || x == W-3 || y == H-3) px[y*W + x] = 0xFF000000;
            else px[y*W + x] = 0xFF101010; // inside
        }
    }
    // prompt çizgisi (turuncu)
    for (int x = 6; x < 20; x++) px[18*W + x] = 0xFFFF6A00;
    // cursor
    px[18*W + 21] = 0xFFFFFFFF;

    vfs_write_all(path, buf, cap);
    kfree(buf);
}

static void draw_demo_icon_card(void) {
    int x = 80;
    int y = 90;
    int w = 72;
    int h = 72;
    int r = 12;

    // shadow
    gfx_fill_round_rect(x + 2, y + 2, w, h, r, 0x101010);

    // mavi kart
    gfx_fill_round_rect(x, y, w, h, r, 0x0078D7);

    // --- ikon üstte (2x scale) ---
    int scale = 2;
    int base_icon_size = 20;
    int icon_w = base_icon_size * scale;
    int icon_h = base_icon_size * scale;

    int pad_top = 8;

    int ix = x + (w - icon_w) / 2;
    int iy = y + pad_top;

    for (int rr = 0; rr < base_icon_size; rr++) {
        for (int cc = 0; cc < base_icon_size; cc++) {

            uint8_t p = folder_icon[rr][cc];

            uint32_t col = 0;
            if (p == 1)      col = 0x000000;
            else if (p == 2) col = 0xFFFFFF;
            else if (p == 3) col = 0xE6E6E6;
            else if (p == 4) col = 0xFFFFFF;

            if (p != 0) {
                // 2x2 büyütme
                for (int sy = 0; sy < scale; sy++) {
                    for (int sx = 0; sx < scale; sx++) {
                        gfx_putpixel(
                            ix + cc * scale + sx,
                            iy + rr * scale + sy,
                            col
                        );
                    }
                }
            }
        }
    }

    // --- yazı kartın içinde altta ---
    const char* label = "Demo";
    int label_len = (int)strlen(label);
    int label_w = label_len * 8;
    int label_h = 16;
    int pad_bottom = 6;

    int text_x = x + (w - label_w) / 2;
    int text_y = y + h - label_h - pad_bottom;

    gfx_draw_text_utf8(text_x, text_y, 0x00FFFFFF, label);
}

static void draw_side_button_right(int px, int py, int ph, int r, const char* text, bool active) {
    // px,py: pencere sağ kenarına yapışık yer
    // ph: buton yüksekliği (örn 44)
    // r: sağ köşe radius (örn 12)

    int bw = 44;  // buton genişliği
    int bx = px - bw;  // sağa yapışık: px pencerenin sağ sınırı (x+w)
    int by = py;

    uint32_t shadow = 0x101010;
    uint32_t bg     = active ? 0x0078D7 : 0x2A2A2A;
    uint32_t txt    = 0x00FFFFFF;

    // shadow (sağa hafif)
    gfx_fill_round_rect4(bx + 2, by + 2, bw, ph, 0, r, 0, r, shadow);

    // buton gövdesi: TL=0, TR=r, BL=0, BR=r
    gfx_fill_round_rect4(bx, by, bw, ph, 0, r, 0, r, bg);

    // yazı ortala (basit)
    if (!text) text = "";
    int len = (int)strlen(text);
    int tw = len * 8;
    int tx = bx + (bw - tw) / 2;
    int ty = by + (ph - 16) / 2;

    gfx_draw_text_utf8(tx, ty, txt, text);
}

static void draw_header_right_chip(int x, int y, int w, int h, int r,
                                   const char* text, bool active) {
    uint32_t shadow = 0x101010;
    uint32_t bg     = active ? 0x0078D7 : 0x2A2A2A;

    // ✅ shadow sadece sağa (alta kaydırma yok)
    gfx_fill_round_rect4(x + 2, y, w, h, 0, r, 0, 0, shadow);

    // body: sadece sağ-üst yuvarlak
    gfx_fill_round_rect4(x, y, w, h, 0, r, 0, 0, bg);

    // text ortala
    if (!text) text = "";
    int len = (int)strlen(text);
    int tw  = len * 8;
    int tx  = x + (w - tw) / 2;
    int ty  = y + (h - 16) / 2;
    gfx_draw_text_utf8(tx, ty, 0x00FFFFFF, text);
}

static void draw_header_flat_chip(int x, int y, int w, int h,
                                  const char* text, bool active) {
    uint32_t shadow = 0x101010;
    uint32_t bg     = active ? 0x0078D7 : 0x2A2A2A;

    // ✅ shadow sadece sağa
    gfx_fill_rect(x + 2, y, w, h, shadow);

    // body düz
    gfx_fill_rect(x, y, w, h, bg);

    // text ortala
    if (!text) text = "";
    int len = (int)strlen(text);
    int tw  = len * 8;
    int tx  = x + (w - tw) / 2;
    int ty  = y + (h - 16) / 2;
    gfx_draw_text_utf8(tx, ty, 0x00FFFFFF, text);
}

static void demo_box_with_header(int x, int y, int w, int h) {
    int r = 20;
    int header_h = 28;

    uint32_t shadow = 0x101010;
    uint32_t body   = 0x2F2F2F;
    uint32_t header = 0x252525;

    // shadow (pencere): altta belli olsun
    gfx_fill_round_rect(x + 2, y + 6, w, h, r, shadow);

    // body (tüm köşeler yuvarlak)
    gfx_fill_round_rect(x, y, w, h, r, body);

    // header: sadece üst köşeler yuvarlak, alt köşeler düz
    gfx_fill_round_rect4(x, y, w, header_h, r, r, 0, 0, header);

    // başlık
    gfx_draw_text_utf8(x + 14, y + 8, 0x00FFFFFF, "KuvixOS Demo");

    // ---- header butonları (sağda 3'lü grup) ----
    int chip_h = header_h;
    int chip_w = 44;
    int gap    = 0; // arada boşluk istemiyorsan 0

    // en sağdaki: ">"
    int x_right = x + w - chip_w;
    // onun solu: "-"
    int x_mid   = x_right - gap - chip_w;
    // onun solu: "+"
    int x_left  = x_mid   - gap - chip_w;

    // iki düz chip
    draw_header_flat_chip(x_left, y, chip_w, chip_h, "+", false);
    draw_header_flat_chip(x_mid,  y, chip_w, chip_h, "-", false);

    // sağ üst köşesi yuvarlak chip
    draw_header_right_chip(x_right, y, chip_w, chip_h, 12, ">", false);
}



// ============================================================
// Desktop Handlers
// ============================================================

void desktop_handle_rename_confirm(const char* new_name) {
    if (rename_target_index < 0) return;

    const char* old_full_path = desktop_icons_get_path(rename_target_index);
    if (!old_full_path || !old_full_path[0]) return;

    char new_full_path[256];

    // Uzantı kontrolü
    if (!strstr(new_name, ".txt")) {
        printk(new_full_path, sizeof(new_full_path),
                 "%s/%s.txt", USER_DESKTOP_PATH, new_name);
    } else {
        printk(new_full_path, sizeof(new_full_path),
                 "%s/%s", USER_DESKTOP_PATH, new_name);
    }

    if (vfs_rename(old_full_path, new_full_path) == 1) {

        notification_show("Isim degistirildi", 800);

        // Eğer yeni dosya oluşturma sonrası açılacaksa
        if (g_open_after_rename) {
            g_open_after_rename = false;
            notepad_open_file(new_full_path);
        }

        desktop_icons_init();
        desktop_icons_snap_all();
    } else {
        notification_show("Isim degistirilemedi!", 1200);
        g_open_after_rename = false;
    }

    rename_target_index = -1;
}

static void desktop_handle_rename(void) {
    rename_target_index = desktop_icons_get_hit(mouse_x, mouse_y);
    if (rename_target_index != -1) {
        desktop_icons_begin_edit(rename_target_index);
    }
}

static void desktop_handle_open(void) {
    int hit = desktop_icons_get_hit(mouse_x, mouse_y);
    if (hit != -1) desktop_icons_process_click(hit);
}

static void desktop_handle_create_file(void) {
    char base[256];
    char final_path[256];

    strcpy(base, USER_DESKTOP_PATH);
    strcat(base, "/yeni_not");

    get_unique_filename(base, ".txt", final_path);

    vfs_file_t* f = 0;
    if (vfs_open(final_path, VFS_O_CREAT | VFS_O_WRONLY, &f) == 1) {
        // ✅ boş dosya oluştur: yazma yok
        vfs_close(f);

        g_open_after_rename = true;
        strncpy(g_open_after_rename_path, final_path, sizeof(g_open_after_rename_path) -1);
        g_open_after_rename_path[sizeof(g_open_after_rename_path) - 1] = '\0';
        
        desktop_icons_init();
        desktop_icons_snap_all();

        int count = desktop_icons_get_count();
        if (count > 0) {
            rename_target_index = count - 1;
            desktop_icons_begin_edit(rename_target_index);
        }
    } else {
        notification_show("Hata: dosya olusturulamadi!", 1500);
    }
}

static void desktop_handle_create_folder(void) {
    char base[256];
    strcpy(base, USER_DESKTOP_PATH);
    strcat(base, "/Yeni_Klasor");

    char final_path[256];
    strcpy(final_path, base);

    int counter = 0;
    vfs_stat_t st;
    while (vfs_stat(final_path, &st)) {
        counter++;
        char num[16];
        simple_itoa(counter, num);
        strcpy(final_path, base);
        strcat(final_path, "_");
        strcat(final_path, num);
    }

    vfs_mkdir(final_path);

    desktop_icons_init();
    desktop_icons_snap_all();
    notification_show("Klasor olusturuldu", 600);
}

static void ctx_open_default(void) {
    if (g_ctx_icon_index < 0) return;
    const char* path = desktop_icons_get_path(g_ctx_icon_index);
    if (path) appmgr_open_path(path);
}

static void ctx_open_with_notepad(void) {
    if (g_ctx_icon_index < 0) return;
    const char* path = desktop_icons_get_path(g_ctx_icon_index);
    if (!path) return;

    app_t* a = appmgr_start_app(3); // APPID_NOTEPAD
    if (a) notepad_open_file(path);
}

static void ctx_open_with_browser(void) {
    if (g_ctx_icon_index < 0) return;
    const char* path = desktop_icons_get_path(g_ctx_icon_index);
    if (!path) return;

    char url[256];
    url[0] = 0;
    strncpy(url, "file:", sizeof(url) - 1);
    url[sizeof(url) - 1] = 0;
    strncat(url, path, sizeof(url) - strlen(url) - 1);

    app_t* b = appmgr_start_app(15); // APPID_BROWSER
    if (b) kuvix_browser_open_url(b, url);
}

static bool ends_with_ci(const char* s, const char* suf) {
    if (!s || !suf) return false;
    int sl = (int)strlen(s);
    int pl = (int)strlen(suf);
    if (sl < pl) return false;

    const char* a = s + (sl - pl);
    for (int i = 0; i < pl; i++) {
        char ca = a[i];
        char cb = suf[i];
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
        if (ca != cb) return false;
    }
    return true;
}

void desktop_reset_selection_state(void) {
    is_selecting = false;
}

// ============================================================
// NEW Desktop API (init + tick + handle_scancode)
// ============================================================

void ui_desktop_init(void) {
    wm_init();
    appmgr_init();
    topbar_init();

    seed_store_repo();
    kbi_write_demo_terminal_icon();
    
    desktop_seed_html_pages();
    desktop_seed_default_shortcuts(false);
    seed_hosts();
    seed_vhosts();

    desktop_icons_init();
    desktop_icons_snap_all();
    
    g_last_btn = 0;
    g_lmb_down = 0;
    g_dragging = 0;
    g_down_hit = -1;
    g_last_click_ms = 0;
    g_last_click_hit = -1;
    is_selecting = false;

    // ✅ ilk frame kesin ekrana basılsın
    g_force_full_present = true;
    g_need_redraw = true;

    // (opsiyonel) diskten kurtarma - burada 1 kere çalışsın
    char disk_buffer[512];
    memset(disk_buffer, 0, 512);

    if (ata_pio_is_ready()) {
        blockdev_t* dev = ata_pio_get_dev();
        ata_pio_read(dev, 2000, disk_buffer, 1);

        if (disk_buffer[0] != '\0' && disk_buffer[0] != (char)0xFF) {
            char rec_path[256];
            strcpy(rec_path, USER_DESKTOP_PATH);
            strcat(rec_path, "/notum.txt");
            if (!file_exists(rec_path)) {
                vfs_file_t* recover_f = 0;
                if (vfs_open(rec_path, VFS_O_CREAT | VFS_O_WRONLY, &recover_f) == 1) {
                    uint32_t written = 0;
                    vfs_write(recover_f, disk_buffer, (uint32_t)strlen(disk_buffer), &written);
                    vfs_close(recover_f);
                    printk("[KuvixOS] Veri diskten notum.txt olarak yuklendi.\n");

                    desktop_icons_init();
                    desktop_icons_snap_all();
                }
            }
        }
    }
}

void ui_desktop_handle_scancode(uint16_t sc)
{
    uint8_t sc8 = (uint8_t)(sc & 0xFF);
    
    char c = 0;
    if (!(sc8 & 0x80)) c = kbd_scancode_to_ascii(sc8);

    // ------------------------------------------------------------
    // GLOBAL HOTKEY: SUPER+R -> Run (app id=7)
    // Set1: R make = 0x13
    // ------------------------------------------------------------
    if (kbd_is_super_pressed() && sc8 == 0x13) {
        app_t* a = appmgr_start_app(7);
        if (a) {
            wm_set_active_id(a->win_id);
            
            // ✅ Hata buradaydı: window_t yerine ui_window_t kullanmalısın
            // ✅ wm_get_window_by_id yerine wm_get_window_ptr (veya senin sistemindeki karşılığı) kullanmalısın
            ui_window_t* win = (ui_window_t*)wm_get_window_ptr(a->win_id);
            
            if (win) {
                desktop_damage_rect(win->x - 10, win->y - 10, win->w + 20, win->h + 20);
                g_need_redraw = true;
            }
        }
        return;
    }

    // ------------------------------------------------------------
    // GLOBAL HOTKEY: SUPER+Q -> Close active window
    // Set1: Q make = 0x10
    // ------------------------------------------------------------
    if (kbd_is_super_pressed() && sc8 == 0x10) { // Q
        int wid = wm_get_active_id();
        wm_request_close(wid);
        desktop_invalidate_full();
        return;
    }

    // ------------------------------------------------------------
    // F12 -> memmon toggle (Set1: 0x58)
    // ------------------------------------------------------------
    if (sc8 == 0x58) {
        memmon_toggle();
        desktop_invalidate_full();
        return;
    }

    // ------------------------------------------------------------
    // F11 -> debug overlay toggle (Set1: 0x57)
    // ------------------------------------------------------------
    if (sc8 == 0x57) {
        g_dbg_overlay = !g_dbg_overlay;
        desktop_invalidate_full();
        return;
    }

    // ------------------------------------------------------------
    // F10 -> removable toggle (Set1: 0x44)  ✅ DOĞRUSU BU
    // ------------------------------------------------------------
    if (sc8 == 0x44) {
        g_removable_plugged = !g_removable_plugged;

        // duration: frame-based (şimdilik)
        if (g_removable_plugged) {
            notification_show("Çıkartılabilir disk takildi", 180);
        } else {
            notification_show("Çıkartılabilir disk cikarildi", 180);
        }

        desktop_invalidate_full();
        return; // app'lere gitmesin
    }

    // ------------------------------------------------------------
    // F9 -> Dirty Region Visualization Toggle (Set1: 0x43)
    // ------------------------------------------------------------
    if (sc8 == 0x43) {
        g_dbg_visualize_dirty = !g_dbg_visualize_dirty;
        notification_show(g_dbg_visualize_dirty ? "Debug: Dirty Rects ON" : "Debug: Dirty Rects OFF", 100);
        desktop_invalidate_full();
        return;
    }

    // ------------------------------------------------------------
    // (İstersen geri açarsın) CTRL+SHIFT+I -> seed shortcuts
    // Set1: I make = 0x17
    // ------------------------------------------------------------
    /*
    if (kbd_is_ctrl_pressed() && kbd_is_shift_pressed() && sc8 == 0x17) {
        desktop_seed_default_shortcuts(false);
        desktop_invalidate_full();
        return;
    }
    */

    printk("[DESKTOP] sc=0x%02x\n", sc8);

    // Modal'lar önce yesin
    if (save_dialog_is_active()) { save_dialog_handle_key(sc, c); desktop_invalidate_full(); return; }
    if (open_dialog_is_active()) { open_dialog_handle_key(sc, c); desktop_invalidate_full(); return; }
    if (messagebox_is_visible()) { return; }

    if (desktop_icons_is_any_editing()) {
        desktop_icons_handle_key(sc, c);
        desktop_invalidate_full();
        return;
    }

    int active_id = wm_get_active_id();
    printk("[DESKTOP] active_win=%d\n", active_id);

    app_t* active_app = appmgr_get_app_by_window_id(active_id);
    printk("[DESKTOP] active_app=%p\n", (void*)active_app);

    if (active_app && active_app->v && active_app->v->on_key) {
        active_app->v->on_key(active_app, sc);
        desktop_invalidate_full();
    }
}

void ui_desktop_tick(void) {
    // 1. Durum Kontrolleri
    topbar_tick(); 
    bool topbar_is_dirty = topbar_consume_dirty();

    int dx, dy, wheel = 0; 
    uint8_t btn;
    static bool hover_dirty = false;
    static int hover_old = -1, hover_new = -1, prev_hover_hit = -2;
    static int last_mouse_x = 0, last_mouse_y = 0;

    bool need_full_present = false;
    bool had_mouse_event = false;

    // Mouse verilerini çek
    ps2_mouse_poll();
    btn = g_last_btn;
    while (ps2_mouse_pop(&dx, &dy, &wheel, &btn)) {
        had_mouse_event = true;
        mouse_x += dx; mouse_y += dy;
        
        // Ekran sınırları
        if (mouse_x < 0) mouse_x = 0; 
        if (mouse_y < 0) mouse_y = 0;
        if (mouse_x > (int)fb_get_width()-1) mouse_x = (int)fb_get_width()-1;
        if (mouse_y > (int)fb_get_height()-1) mouse_y = (int)fb_get_height()-1;

        // 1. ADIM: Mouse koordinatlarında bir pencere var mı?
        int win_id = wm_find_window_at(mouse_x, mouse_y);
        bool is_over_window = (win_id != -1);

        if (wheel != 0) {
            int step = (wheel > 0) ? -1 : 1;
            wm_handle_mouse_wheel(mouse_x, mouse_y, step, btn);
            desktop_request_redraw();
        }
        
        if (dx != 0 || dy != 0) {
            // Hover kontrolü: Sadece pencere üstünde değilsek ikonlara bak
            int current_hover = -1;
            if (!is_over_window && !context_menu_is_visible()) {
                current_hover = desktop_icons_get_hit(mouse_x, mouse_y);
            }

            if (current_hover != prev_hover_hit) {
                hover_old = prev_hover_hit; 
                hover_new = current_hover; 
                hover_dirty = true;
                prev_hover_hit = current_hover; 
                g_need_redraw = true;
                
                int x, y, w, h;
                if (desktop_icons_get_rect(hover_old, &x, &y, &w, &h)) desktop_damage_rect(x, y, w, h);
                if (desktop_icons_get_rect(hover_new, &x, &y, &w, &h)) desktop_damage_rect(x, y, w, h);
            }

            if (context_menu_is_visible()) {
                context_menu_handle_mouse(mouse_x, mouse_y, false);
            }

            wm_handle_mouse_move(mouse_x, mouse_y);
            
            // Sürükleme mantığı (Sadece masaüstü ikonları içinse)
            if (g_lmb_down && !is_over_window) {
                if (g_down_hit != -1) {
                    if (!g_dragging && (abs(mouse_x - g_down_x) > 4 || abs(mouse_y - g_down_y) > 4)) {
                        g_dragging = true;
                        if (!desktop_icons_is_selected(g_down_hit)) {
                            desktop_icons_deselect_all();
                            desktop_icons_select(g_down_hit);
                        }
                    }
                    if (g_dragging) {
                        desktop_icons_move_selected(mouse_x - last_mouse_x, mouse_y - last_mouse_y);
                        g_need_redraw = true;
                    }
                } else if (is_selecting) {
                    desktop_damage_rect(min(sel_start_x, mouse_x) - 2, min(sel_start_y, mouse_y) - 2, 
                                        abs(mouse_x - sel_start_x) + 4, abs(mouse_y - sel_start_y) + 4);
                    desktop_icons_select_in_rect(sel_start_x, sel_start_y, mouse_x, mouse_y);
                    g_need_redraw = true;
                }
            }        
        }

        uint8_t pressed = btn & ~g_last_btn;
        uint8_t released = g_last_btn & ~btn;

        // Context Menu Click (En yüksek öncelik)
        if (context_menu_is_visible()) {
            if (pressed & 1 || pressed & 2) {
                context_menu_handle_mouse(mouse_x, mouse_y, true);
                if (!context_menu_is_visible()) need_full_present = true;
                goto end_loop; // Olay tüketildi
            }
        }

        // 2. ADIM: PENCERE ETKİLEŞİMİ (Input Routing)
        if (is_over_window) {
            // Eğer bir pencereye tıklandıysa, masaüstü seçimlerini temizle (isteğe bağlı)
            // desktop_icons_deselect_all(); 

            if (pressed & 2) { // Sağ Tık
                if (wm_is_titlebar_hit(win_id, mouse_x, mouse_y)) {
                    context_menu_reset();
                    // Doğrudan wm_close_active_window kullanabilirsin 
                    // çünkü win_id zaten az önce bring_to_front ile aktif yapılmış olmalı
                    context_menu_add_item("Kapat", wm_close_active_window);
                    context_menu_add_item("Kucult", NULL); // wm_minimize_active_window da ekleyebilirsin
                    context_menu_show(mouse_x, mouse_y);
                } else {
                    // Pencere içine sağ tık -> Pencereye ilet
                    wm_handle_mouse(mouse_x, mouse_y, pressed, released, btn);
                }
            } else {
                // Sol tık veya diğerleri -> WM'ye gönder
                wm_handle_mouse(mouse_x, mouse_y, pressed, released, btn);
            }
            goto end_loop; // Olay pencere tarafından yutuldu, masaüstüne geçme
        }

        // 3. ADIM: MASAÜSTÜ ETKİLEŞİMİ (Buraya gelindiyse pencereye tıklanmamıştır)
        if (pressed & 1) {
            int hit = desktop_icons_get_hit(mouse_x, mouse_y);
            uint32_t current_time = g_ticks_ms;

            if (hit != -1 && hit == last_hit_index && (current_time - last_click_time) < DBLCLICK_MS) {
                desktop_icons_process_click(hit);
                g_lmb_down = 0; g_down_hit = -1;
                last_click_time = 0; last_hit_index = -1;
            } else {
                g_lmb_down = 1; g_down_x = mouse_x; g_down_y = mouse_y; g_down_hit = hit;
                last_click_time = current_time; last_hit_index = hit;

                if (hit != -1) {
                    if (!desktop_icons_is_selected(hit)) {
                        if (!kbd_is_ctrl_pressed()) desktop_icons_deselect_all();
                        desktop_icons_select(hit);
                    }
                    is_selecting = false; 
                } else {
                    if (!kbd_is_ctrl_pressed()) desktop_icons_deselect_all();
                    is_selecting = true;
                    sel_start_x = mouse_x; sel_start_y = mouse_y;
                }
            }
        }

        if (released & 1) {
            if (g_dragging) {
                g_dragging = false;
                desktop_icons_snap_all();
                need_full_present = true;
            }
            is_selecting = false;
            g_lmb_down = 0; g_down_hit = -1;
        }

        if (pressed & 2) { // Masaüstü Sağ Tık
            context_menu_reset();
            int hit = desktop_icons_get_hit(mouse_x, mouse_y);
            if (hit != -1) {
                g_ctx_icon_index = hit;
                context_menu_add_item("Ac", ctx_open_default);
                context_menu_add_item("Ozellikler", NULL);
            } else {
                context_menu_add_item("Yeni Metin Belgesi", desktop_handle_create_file);
                context_menu_add_item("Masaustunu Yenile", desktop_invalidate_full);
            }
            context_menu_show(mouse_x, mouse_y);
        }

    end_loop:
        g_last_btn = btn;
        last_mouse_x = mouse_x; last_mouse_y = mouse_y;
        if (pressed & 1 || pressed & 2 || released & 1) g_need_redraw = true;
    }

    appmgr_update_all();

    // --- RENDER VE SUNUM KARARI ---
    if (topbar_is_dirty || g_force_full_present) g_need_redraw = true; 
    
    if (!g_need_redraw && !appmgr_any_continuous_redraw() && !g_dbg_overlay) {
        if (had_mouse_event) cursor_overlay_step(mouse_x, mouse_y, false);
        return;
    }

    g_need_redraw = false;
    bool force_full = g_force_full_present; g_force_full_present = false;

    // --- ÇİZİM ---
    fb_clear(ui_get_desktop_bg());
    desktop_icons_draw_all();
    
    if (is_selecting) {
        int rx = min(sel_start_x, mouse_x); int ry = min(sel_start_y, mouse_y);
        int rw = abs(mouse_x - sel_start_x) + 1; int rh = abs(mouse_y - sel_start_y) + 1;
        gfx_draw_alpha_rect(rw, rh, 0, 85, 170, 150, rx, ry);
        gfx_draw_rect(rx, ry, rw, rh, 0x00AAFF);
    }
    
    topbar_draw();
    wm_draw();
    if (memmon_is_visible()) memmon_draw(fb_get_width(), fb_get_height());
    save_dialog_draw(); open_dialog_draw(); 
    context_menu_draw();
    messagebox_draw(); notification_draw(); dbg_draw_panel();
    
    cursor_overlay_step(mouse_x, mouse_y, true);

    // ✅ AKILLI PRESENT (DARBOĞAZI ÖNLER)
    if (force_full || need_full_present || context_menu_is_visible() || is_selecting || g_dragging) {
        fb_present(); // Bu durumlarda full screen redraw kaçınılmaz
    } else {
        // Sadece hasar gören ikonlar ve seçim alanı
        int dx0, dy0, dw0, dh0;
        if (desktop_consume_damage_rect(&dx0, &dy0, &dw0, &dh0)) {
            present_rect_safe(dx0, dy0, dw0, dh0);
        }
        
        // Hover geçişlerini anında bas
        if (hover_dirty) {
            int x, y, w, h;
            if (desktop_icons_get_rect(hover_old, &x, &y, &w, &h)) present_rect_safe(x-5, y-5, w+10, h+10);
            if (desktop_icons_get_rect(hover_new, &x, &y, &w, &h)) present_rect_safe(x-5, y-5, w+10, h+10);
            hover_dirty = false;
        }
        
        // Saat güncellemesi için sadece topbar'ı bas
        if (topbar_is_dirty) {
            present_rect_safe(0, 0, (int)fb_get_width(), 28);
        }
    }
}

// ============================================================
// Legacy blocking API (kalsın ama kullanma)
// ============================================================
/*
void ui_desktop_run(void) {
    ui_desktop_init();
    while (1) {
        ui_desktop_tick();
        asm volatile("hlt");
    }
}*/