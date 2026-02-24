// kernel/ui/window.c
#include <ui/window/window.h>       // ui_window_t
#include <ui/theme.h>               // ui_get_theme(), ui_theme_t
#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/memory/kmalloc.h>
#include <lib/string.h>
#include <stdint.h>
#include <stdbool.h>

// ------------------------------------------------------------
// Small helpers (clamp / rect / text width)
// ------------------------------------------------------------
static int clampi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static int pt_in_rect(int px, int py, int x, int y, int w, int h) {
    return (px >= x && py >= y && px < x + w && py < y + h);
}

// 8px font varsayımı. Sende UTF8 genişlik hesabı varsa onu kullan.
static int text_width8(const char* s) {
    if (!s) return 0;
    return (int)strlen(s) * 8;
}

// Title align: left/center/right (theme’de nasıl tutuyorsan ona göre)
static int calc_title_x(const ui_theme_t* th, int safe_x, int safe_w, int text_w) {
    (void)th;
    // default left
    int tx = safe_x;
    // safe alanın dışına taşmasın
    if (text_w > safe_w) tx = safe_x;
    return tx;
}

// ------------------------------------------------------------
// Icons (16x16 ARGB key blit)
// ------------------------------------------------------------
// Sende bu ikonlar zaten var demiştin:
extern const uint32_t* ui_icon_close_16(void);
extern const uint32_t* ui_icon_max_16(void);
extern const uint32_t* ui_icon_min_16(void);

static void blit_icon_centered(int bx, int by, int bsz, const uint32_t* icon16) {
    if (!icon16) return;
    int ix = bx + (bsz - 16) / 2;
    int iy = by + (bsz - 16) / 2;
    fb_blit_argb_key(ix, iy, 16, 16, icon16, 0x00000000);
}

// ------------------------------------------------------------
// Caption button background selection
// (Sende theme’de capbtn_* alanları var gibi; yoksa basit fallback)
// which: 0 close, 1 max, 2 min
// ------------------------------------------------------------
static uint32_t capbtn_bg_for(const ui_theme_t* th, int which, int hover, int press) {
    (void)which;
    if (!th) return 0;

    // Örnek fallback mantık:
    // - press: biraz koyu
    // - hover: biraz açık
    // - normal: 0 => çizme (title_bg üstünde durur)
    if (press) return 0x00303030;
    if (hover) return 0x00404040;

    // istersen close için kırmızımsı hover vs yaparsın
    // if (which == 0 && hover) return 0x00402020;

    return 0;
}

// ------------------------------------------------------------
// ✅ ROUND CORNER SAVE/RESTORE (üst üste pencerelerde doğru köşe)
// ------------------------------------------------------------
typedef struct {
    int r;
    uint32_t* tl;
    uint32_t* tr;
    uint32_t* bl;
    uint32_t* br;
} corner_cache_t;

static corner_cache_t g_cc = {0};

static void cc_free(void) {
    if (g_cc.tl) kfree(g_cc.tl);
    if (g_cc.tr) kfree(g_cc.tr);
    if (g_cc.bl) kfree(g_cc.bl);
    if (g_cc.br) kfree(g_cc.br);
    g_cc.tl = g_cc.tr = g_cc.bl = g_cc.br = 0;
    g_cc.r = 0;
}

static void cc_ensure(int r) {
    if (r <= 0) return;
    if (g_cc.r == r && g_cc.tl) return;

    cc_free();

    int n = r * r;
    g_cc.tl = (uint32_t*)kmalloc((uint32_t)n * 4);
    g_cc.tr = (uint32_t*)kmalloc((uint32_t)n * 4);
    g_cc.bl = (uint32_t*)kmalloc((uint32_t)n * 4);
    g_cc.br = (uint32_t*)kmalloc((uint32_t)n * 4);

    if (!g_cc.tl || !g_cc.tr || !g_cc.bl || !g_cc.br) {
        cc_free();
        return;
    }

    g_cc.r = r;
}

static inline int outside_round(int xx, int yy, int r) {
    // çeyrek daire maskesi: dairenin DIŞI restore edilecek
    int dx = (r - 1) - xx;
    int dy = (r - 1) - yy;
    return (dx*dx + dy*dy) >= (r*r);
}

static void cc_save(int x, int y, int w, int h, int r) {
    if (r <= 0) return;
    if (w < r || h < r) return;

    cc_ensure(r);
    if (!g_cc.tl) return;

    int x0 = x, y0 = y;
    int x1 = x + w - r;
    int y1 = y + h - r;

    for (int yy = 0; yy < r; yy++) {
        for (int xx = 0; xx < r; xx++) {
            int i = yy * r + xx;
            g_cc.tl[i] = fb_getpixel(x0 + xx, y0 + yy);
            g_cc.tr[i] = fb_getpixel(x1 + xx, y0 + yy);
            g_cc.bl[i] = fb_getpixel(x0 + xx, y1 + yy);
            g_cc.br[i] = fb_getpixel(x1 + xx, y1 + yy);
        }
    }
}

static void cc_restore(int x, int y, int w, int h, int r) {
    if (r <= 0) return;
    if (!g_cc.tl || g_cc.r != r) return;
    if (w < r || h < r) return;

    int x0 = x, y0 = y;
    int x1 = x + w - r;
    int y1 = y + h - r;

    for (int yy = 0; yy < r; yy++) {
        for (int xx = 0; xx < r; xx++) {
            if (!outside_round(xx, yy, r)) continue;

            int i = yy * r + xx;
            fb_putpixel(x0 + xx, y0 + yy, g_cc.tl[i]);
            fb_putpixel(x1 + xx, y0 + yy, g_cc.tr[i]);
            fb_putpixel(x0 + xx, y1 + yy, g_cc.bl[i]);
            fb_putpixel(x1 + xx, y1 + yy, g_cc.br[i]);
        }
    }
}

ui_rect_t ui_window_get_content_rect(const ui_window_t* win) {
    ui_rect_t r = {0,0,0,0};
    if (!win) return r;

    const ui_theme_t* th = ui_get_theme();
    if (!th) return r;

    int border  = clampi(th->window_border_px, 0, 16);
    int title_h = clampi(th->window_title_h, 18, (win->h > 18 ? win->h : 18));

    // 🔴 DİKKAT:
    // x border’dan başlasın
    // ama width border düşülmesin
    // çünkü app sağ/sol boşluğu kaplayacak

    r.x = win->x;
    r.y = win->y + title_h;

    r.w = win->w;
    r.h = win->h - title_h;

    if (r.w < 0) r.w = 0;
    if (r.h < 0) r.h = 0;

    return r;
}

// ------------------------------------------------------------
// Public API: draw window chrome
// ------------------------------------------------------------
void ui_window_draw(const ui_window_t* win, int is_active, int mx, int my) {
    (void)is_active;
    if (!win) return;

    const ui_theme_t* th = ui_get_theme();
    if (!th) return;

    // --- Theme clamps
    int border  = clampi(th->window_border_px, 0, 16);
    int title_h = clampi(th->window_title_h, 18, (win->h > 18 ? win->h : 18));
    int btn     = clampi(th->window_btn_size, 12, title_h - 4);

    int gap   = clampi(th->window_btn_gap, 2, 24);
    int mar   = clampi(th->window_btn_margin, 2, 64);
    int pad_l = clampi(th->window_title_pad_l, 4, 64);
    int pad_r = clampi(th->window_title_pad_r, 4, 64);

    int btn_pad_l = clampi(th->window_btn_pad_left, 0, 128);
    int btn_pad_r = clampi(th->window_btn_pad_right, 0, 128);

    int radius = clampi(th->window_corner_radius, 0, 24);

    const uint32_t col_win_bg     = (uint32_t)th->window_bg;
    const uint32_t col_border     = (uint32_t)th->window_border;
    const uint32_t col_title_bg   = (uint32_t)th->window_title_bg;
    const uint32_t col_title_text = (uint32_t)th->window_title_text;

    // ✅ pencereyi çizmeden önce arka planı kaydet
    // if (radius > 0) cc_save(win->x, win->y, win->w, win->h, radius);

    // --- Window body
    fb_draw_rect(win->x, win->y, win->w, win->h, col_win_bg);

    // Border
    for (int i = 0; i < border; i++) {
        int w = win->w - 2 * i;
        int h = win->h - 2 * i;
        if (w <= 0 || h <= 0) break;
        fb_draw_rect_outline(win->x + i, win->y + i, w, h, col_border);
    }

    // --- Title bar (tam genişlik!)
    fb_draw_rect(win->x, win->y, win->w, title_h, col_title_bg);

    // --- Buttons layout
    const int btn_total_w = (btn * 3) + (gap * 2);

    int by = win->y + (title_h - btn) / 2;
    if (by < win->y) by = win->y;

    bool btn_right = (th->window_btn_layout == UI_BTN_LAYOUT_RIGHT);

    int bx0 = btn_right
        ? (win->x + win->w - mar - btn)
        : (win->x + mar);

    // order validate
    uint8_t o0 = th->window_btn_order[0];
    uint8_t o1 = th->window_btn_order[1];
    uint8_t o2 = th->window_btn_order[2];

    bool bad = (o0 > 2 || o1 > 2 || o2 > 2) || (o0 == o1) || (o0 == o2) || (o1 == o2);
    uint8_t order[3];
    if (bad) { order[0] = 0; order[1] = 1; order[2] = 2; }
    else     { order[0] = o0; order[1] = o1; order[2] = o2; }

    // index: 0 close, 1 max, 2 min
    int btn_x[3] = {0, 0, 0};

    int x = bx0;
    for (int slot = 0; slot < 3; slot++) {
        uint8_t which = order[slot];
        btn_x[which] = x;
        if (btn_right) x -= (btn + gap);
        else           x += (btn + gap);
    }

    // Hover checks
    int hover_close = pt_in_rect(mx, my, btn_x[0], by, btn, btn);
    int hover_max   = pt_in_rect(mx, my, btn_x[1], by, btn, btn);
    int hover_min   = pt_in_rect(mx, my, btn_x[2], by, btn, btn);

    // Press state: bunu WM input’undan beslersin.
    int press_close = 0, press_max = 0, press_min = 0;

    // Draw caption button backgrounds + optional outlines
    for (int which = 0; which < 3; which++) {
        int hover = 0, press = 0;
        if (which == 0) { hover = hover_close; press = press_close; }
        if (which == 1) { hover = hover_max;   press = press_max; }
        if (which == 2) { hover = hover_min;   press = press_min; }

        uint32_t bg = capbtn_bg_for(th, which, hover, press);
        if (bg != 0) {
            fb_draw_rect(btn_x[which], by, btn, btn, bg);
        }

        if (th->capbtn_outline_px) {
            fb_draw_rect_outline(btn_x[which], by, btn, btn, col_border);
        }
    }

    // Icons
    if (th->capbtn_icon_enabled) {
        blit_icon_centered(btn_x[0], by, btn, ui_icon_close_16());
        blit_icon_centered(btn_x[1], by, btn, ui_icon_max_16());
        blit_icon_centered(btn_x[2], by, btn, ui_icon_min_16());
    }

    // --- Title safe area (butonların kapladığı alanı düş)
    int safe_x = win->x + pad_l;
    int safe_w = win->w - (pad_l + pad_r);

    if (btn_right) {
        int right_block = mar + btn_total_w + btn_pad_r;
        safe_w = win->w - pad_l - right_block;
    } else {
        int left_block = mar + btn_total_w + btn_pad_l;
        safe_x = win->x + left_block;
        safe_w = win->w - left_block - pad_r;
    }
    if (safe_w < 0) safe_w = 0;

    // --- Title text
    if (win->title && win->title[0]) {
        int text_px_w = text_width8(win->title);
        int tx = calc_title_x(th, safe_x, safe_w, text_px_w);

        const int font_h = 16;
        int ty = win->y + (title_h - font_h) / 2;
        if (ty < win->y) ty = win->y;

        gfx_draw_text_utf8(tx, ty, col_title_text, win->title);
    }

    // ✅ en sonda arka planı geri koyarak gerçek yuvarlak köşe yap
    if (radius > 0) cc_restore(win->x, win->y, win->w, win->h, radius);
}