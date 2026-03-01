// kernel/ui/apps/kbi_viewer.c

#include <ui/apps/kbi_viewer.h>

#include <app/app.h>
#include <ui/wm.h>

#include <kernel/fs/vfs.h>
#include <kernel/memory/kmalloc.h>

#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/video/fb.h>

#include <lib/string.h>
#include <stdint.h>
#include <stdbool.h>

// terminal'deki gibi
extern char kbd_scancode_to_ascii(uint8_t scancode);

#define KBI_MAGIC 0x3149424B /* 'KBI1' */

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;     // 'KBI1'
    uint16_t w;
    uint16_t h;
    uint32_t flags;     // bit0=1 => ARGB8888
    uint32_t reserved;  // 0
} kbi_hdr_t;
#pragma pack(pop)

typedef struct {
    uint16_t w, h;
    uint32_t* pixels;
} kbi_image_t;

static kbi_viewer_t* st(app_t* app) {
    return (app && app->user) ? (kbi_viewer_t*)app->user : 0;
}

static void kbi_free(kbi_image_t* img) {
    if (!img) return;
    if (img->pixels) {
        kfree(img->pixels);
        img->pixels = 0;
    }
    img->w = img->h = 0;
}

static bool kbi_load_file(const char* path, kbi_image_t* out) {
    if (!out) return false;
    out->w = 0;
    out->h = 0;
    out->pixels = 0;

    if (!path || !path[0]) return false;

    // dosya boyu
    vfs_stat_t stt;
    if (!vfs_stat(path, &stt)) return false;
    if (stt.type != VFS_T_FILE) return false;

    uint32_t cap = stt.size;
    if (cap < sizeof(kbi_hdr_t)) return false;

    // RAM'e oku
    uint8_t* data = (uint8_t*)kmalloc(cap);
    if (!data) return false;

    uint32_t sz = 0;
    if (!vfs_read_all(path, data, cap, &sz) || sz < sizeof(kbi_hdr_t)) {
        kfree(data);
        return false;
    }

    kbi_hdr_t hdr;
    memcpy(&hdr, data, sizeof(hdr));

    if (hdr.magic != KBI_MAGIC || hdr.w == 0 || hdr.h == 0) {
        kfree(data);
        return false;
    }
    if ((hdr.flags & 1u) == 0) { // ARGB flag yok
        kfree(data);
        return false;
    }

    uint32_t pix_count = (uint32_t)hdr.w * (uint32_t)hdr.h;
    uint32_t need = (uint32_t)sizeof(kbi_hdr_t) + pix_count * 4u;
    if (sz < need) {
        kfree(data);
        return false;
    }

    uint32_t* pix = (uint32_t*)kmalloc(pix_count * 4u);
    if (!pix) {
        kfree(data);
        return false;
    }

    memcpy(pix, data + sizeof(kbi_hdr_t), pix_count * 4u);
    kfree(data);

    out->w = hdr.w;
    out->h = hdr.h;
    out->pixels = pix;
    return true;
}

static void draw_checker_bg(int w, int h) {
    const int tile = 12;
    for (int y = 0; y < h; y += tile) {
        for (int x = 0; x < w; x += tile) {
            int odd = ((x / tile) ^ (y / tile)) & 1;
            uint32_t c = odd ? 0x2A2A2A : 0x1C1C1C;
            int tw = (x + tile <= w) ? tile : (w - x);
            int th = (y + tile <= h) ? tile : (h - y);
            gfx_fill_rect(x, y, tw, th, c);
        }
    }
}

// ARGB8888 alpha blend (gfx_putpixel_alpha kullanarak)
static void draw_kbi_image_alpha(const kbi_image_t* img, int dx, int dy) {
    if (!img || !img->pixels) return;

    int W = (int)fb_get_width();
    int H = (int)fb_get_height();

    // Biz app içinde client origin'deyiz (wm gfx_set_origin yapıyor)
    // Sınır kontrolü için yine de basit clip yapalım (client zaten screen’e mapleniyor)
    (void)W; (void)H;

    for (int y = 0; y < (int)img->h; y++) {
        for (int x = 0; x < (int)img->w; x++) {
            uint32_t p = img->pixels[y * (int)img->w + x];

            uint8_t a = (uint8_t)((p >> 24) & 0xFF);
            if (a == 0) continue;

            uint8_t r = (uint8_t)((p >> 16) & 0xFF);
            uint8_t g = (uint8_t)((p >> 8)  & 0xFF);
            uint8_t b = (uint8_t)((p)       & 0xFF);

            if (a == 255) {
                gfx_putpixel(dx + x, dy + y, (uint32_t)((r << 16) | (g << 8) | b));
            } else {
                gfx_putpixel_alpha(dx + x, dy + y, r, g, b, a);
            }
        }
    }
}

static void viewer_reload(kbi_viewer_t* v) {
    if (!v) return;

    kbi_image_t img = { v->w, v->h, v->pixels };
    kbi_free(&img);

    v->w = v->h = 0;
    v->pixels = 0;
    v->loaded = 0;
    v->failed = 0;

    kbi_image_t out = {0,0,0};
    if (kbi_load_file(v->path, &out)) {
        v->w = out.w;
        v->h = out.h;
        v->pixels = out.pixels;
        v->loaded = 1;
        v->failed = 0;

        // ilk açılışta ortala
        v->pan_x = 0;
        v->pan_y = 0;
    } else {
        v->failed = 1;
    }
}

// ------------------------------------------------------------
// vtbl
// ------------------------------------------------------------

static void on_create(app_t* app) {
    kbi_viewer_t* v = st(app);
    if (!v) return;

    memset(v, 0, sizeof(*v));

    // Şimdilik default bir ikon dosyası (varsa)
    // İstersen bunu /system/icons/terminal.kbi yap
    strncpy(v->path, "/system/icons/terminal.kbi", sizeof(v->path) - 1);
    v->path[sizeof(v->path) - 1] = 0;

    viewer_reload(v);
}

static void on_destroy(app_t* app) {
    kbi_viewer_t* v = st(app);
    if (!v) return;

    kbi_image_t img = { v->w, v->h, v->pixels };
    kbi_free(&img);

    v->pixels = 0;
    v->w = v->h = 0;
}

static void on_draw(app_t* app) {
    kbi_viewer_t* v = st(app);
    if (!v) return;

    ui_rect_t cr = wm_get_client_rect(app->win_id);
    v->cw = cr.w;
    v->ch = cr.h;

    // background
    draw_checker_bg(cr.w, cr.h);

    // üst info bar
    gfx_fill_rect(0, 0, cr.w, 20, 0x101010);
    gfx_draw_rect(0, 0, cr.w, 20, 0x303030);

    if (v->loaded) {
        char buf[200];
        buf[0] = 0;
        strcat(buf, "KBI Viewer  ");
        strcat(buf, v->path);
        gfx_draw_text_utf8(8, 3, 0xE0E0E0, buf);
    } else if (v->failed) {
        gfx_draw_text_utf8(8, 3, 0xFF6A00, "KBI Viewer - load failed (R=reload)");
        gfx_draw_text_utf8(8, 22, 0xC0C0C0, v->path);
        return;
    } else {
        gfx_draw_text_utf8(8, 3, 0xC0C0C0, "KBI Viewer - loading...");
        return;
    }

    // image area y offset (info bar)
    int top = 24;

    int img_w = (int)v->w;
    int img_h = (int)v->h;

    // merkez + pan
    int dx = (cr.w - img_w) / 2 + v->pan_x;
    int dy = top + (cr.h - top - img_h) / 2 + v->pan_y;

    // image
    kbi_image_t img = { v->w, v->h, v->pixels };
    draw_kbi_image_alpha(&img, dx, dy);

    // border
    gfx_draw_rect(dx - 1, dy - 1, img_w + 2, img_h + 2, 0x404040);

    // help text
    gfx_draw_text_utf8(8, cr.h - 18, 0x808080, "Mouse drag = pan   R = reload");
}

static void on_mouse(app_t* app, int mx, int my, uint8_t pr, uint8_t rel, uint8_t btn) {
    kbi_viewer_t* v = st(app);
    if (!v) return;

    // left press
    if (pr & 1) {
        v->dragging = 1;
        v->drag_last_x = mx;
        v->drag_last_y = my;
    }

    // dragging
    if (v->dragging && (btn & 1)) {
        int dx = mx - v->drag_last_x;
        int dy = my - v->drag_last_y;

        v->pan_x += dx;
        v->pan_y += dy;

        v->drag_last_x = mx;
        v->drag_last_y = my;
    }

    // left release
    if (rel & 1) {
        v->dragging = 0;
    }
}

static void on_key(app_t* app, uint16_t keyev) {
    kbi_viewer_t* v = st(app);
    if (!v) return;

    uint8_t sc = (uint8_t)(keyev & 0xFF);
    if (sc & 0x80) return; // break ignore

    // 'R' (set1) genelde 0x13
    if (sc == 0x13) {
        viewer_reload(v);
        return;
    }

    // arrow pan (set1): up=0x48 down=0x50 left=0x4B right=0x4D
    if (sc == 0x4B) v->pan_x -= 8;
    if (sc == 0x4D) v->pan_x += 8;
    if (sc == 0x48) v->pan_y -= 8;
    if (sc == 0x50) v->pan_y += 8;

    // küçük bonus: ESC çık (istersen)
    // if (sc == 0x01) wm_close_window(app->win_id);
}

const app_vtbl_t kbi_viewer_vtbl = {
    .on_create  = on_create,
    .on_destroy = on_destroy,
    .on_mouse   = on_mouse,
    .on_key     = on_key,
    .on_draw    = on_draw
};