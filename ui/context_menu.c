// kernel/ui/context_menu.c
#include <ui/context_menu.h>
#include <ui/desktop.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/video/fb.h>
#include <lib/string.h>
#include <stdbool.h>

#define MAX_MENU_ITEMS 16

#define ITEM_H   20
#define PAD      4
#define ARROW_W  12
#define CHECK_W  12

// TEST MODE: sadece beyaz kutu çiz
#define CONTEXT_MENU_TEST_RECT 0
#define TEST_W  220
#define TEST_H  140

typedef struct context_menu context_menu_t;

typedef struct {
    char text[32];
    void (*callback)(void);
    context_menu_t* submenu;  // varsa ">"
} menu_item_t;

struct context_menu {
    menu_item_t items[MAX_MENU_ITEMS];
    int count;

    int x, y, w, h;
    int hover;

    context_menu_t* parent;
    context_menu_t* child;
    int parent_item_index;

    bool visible;
};

static context_menu_t g_root;

#define SUBMENU_POOL_MAX 8
static context_menu_t g_pool[SUBMENU_POOL_MAX];
static int g_pool_used = 0;

// ------------------------------------------------------------
// small helpers
// ------------------------------------------------------------
static int clampi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static int text_px(const char* s) {
    return (int)strlen(s) * 8; // 8px font varsayımı
}

static void menu_measure(context_menu_t* m) {
    int maxw = 0;
    for (int i = 0; i < m->count; i++) {
        int w = text_px(m->items[i].text);
        if (w > maxw) maxw = w;
    }

    m->w = PAD + CHECK_W + 6 + maxw + 6 + ARROW_W + PAD;
    m->h = PAD * 2 + m->count * ITEM_H;

    if (m->w < 120) m->w = 120;
    if (m->h < (PAD * 2 + ITEM_H)) m->h = (PAD * 2 + ITEM_H); // min yükseklik
}

static bool menu_hit(context_menu_t* m, int mx, int my) {
    return (mx >= m->x && mx < (m->x + m->w) &&
            my >= m->y && my < (m->y + m->h));
}

static int menu_item_at(context_menu_t* m, int mx, int my) {
    if (!menu_hit(m, mx, my)) return -1;
    int rel_y = my - m->y - PAD;
    if (rel_y < 0) return -1;
    int idx = rel_y / ITEM_H;
    if (idx < 0 || idx >= m->count) return -1;
    return idx;
}

// normal: ölçer + clamp
static void menu_place_root_measured(context_menu_t* m, int x, int y) {
    int sw = fb_get_width();
    int sh = fb_get_height();

    menu_measure(m);

    int nx = x;
    int ny = y;

    if (nx + m->w > sw) nx = sw - m->w;
    if (ny + m->h > sh) ny = sh - m->h;

    nx = clampi(nx, 0, sw - m->w);
    ny = clampi(ny, 0, sh - m->h);

    m->x = nx;
    m->y = ny;
}

// TEST: ölçmeden (mevcut w/h ile) clamp
static void menu_place_root_raw(context_menu_t* m, int x, int y) {
    int sw = fb_get_width();
    int sh = fb_get_height();

    int nx = x;
    int ny = y;

    if (nx + m->w > sw) nx = sw - m->w;
    if (ny + m->h > sh) ny = sh - m->h;

    nx = clampi(nx, 0, sw - m->w);
    ny = clampi(ny, 0, sh - m->h);

    m->x = nx;
    m->y = ny;
}

static void menu_place_submenu(context_menu_t* parent, context_menu_t* child, int item_index) {
    int sw = fb_get_width();
    int sh = fb_get_height();

    menu_measure(child);

    int anchor_y = parent->y + PAD + item_index * ITEM_H;
    int y = anchor_y;

    int x_right = parent->x + parent->w;
    int x_left  = parent->x - child->w;

    int x = x_right;
    if (x + child->w > sw) x = x_left;
    if (x < 0) x = 0;

    if (y + child->h > sh) y = sh - child->h;
    if (y < 0) y = 0;

    child->x = x;
    child->y = y;
}

static void menu_close_child(context_menu_t* m) {
    if (!m) return;
    if (m->child) {
        m->child->visible = false;
        m->child->parent = NULL;
        m->child = NULL;
    }
}

// ------------------------------------------------------------
// PUBLIC API
// ------------------------------------------------------------
void context_menu_reset(void) {
    g_root.count = 0;
    g_root.hover = -1;
    g_root.visible = false;
    g_root.parent = NULL;
    g_root.child = NULL;

    g_pool_used = 0; // ✅ submenu pool reset
}

void context_menu_add_item(const char* text, void (*callback)(void)) {
    context_menu_add_item_to(&g_root, text, callback);
}

void context_menu_add_item_to(context_menu_t* menu, const char* text, void (*callback)(void)) {
    if (!menu) return;
    if (menu->count >= MAX_MENU_ITEMS) return;

    menu_item_t* it = &menu->items[menu->count++];
    memset(it, 0, sizeof(*it));
    strncpy(it->text, text ? text : "", 31);
    it->text[31] = '\0';
    it->callback = callback;
    it->submenu = NULL;
}

context_menu_t* context_menu_add_submenu(const char* text) {
    return context_menu_add_submenu_to(&g_root, text);
}

context_menu_t* context_menu_add_submenu_to(context_menu_t* menu, const char* text) {
    if (!menu) return NULL;
    if (menu->count >= MAX_MENU_ITEMS) return NULL;

    // ✅ pool'dan submenu al
    if (g_pool_used >= SUBMENU_POOL_MAX) return NULL;
    context_menu_t* child = &g_pool[g_pool_used++];

    child->count = 0;
    child->hover = -1;
    child->visible = false;
    child->parent = NULL;
    child->child = NULL;

    menu_item_t* it = &menu->items[menu->count++];
    memset(it, 0, sizeof(*it));
    strncpy(it->text, text ? text : "", 31);
    it->text[31] = '\0';
    it->callback = NULL;
    it->submenu = child;

    return child;
}

void context_menu_show(int x, int y) {
    // 1. ESKİ MENÜYÜ TEMİZLE: Eğer menü zaten açıksa, eski yerini damage işaretle
    if (g_root.visible) {
        desktop_damage_rect(g_root.x, g_root.y, g_root.w, g_root.h);
    }

    g_root.visible = true;
    g_root.hover = -1;
    menu_close_child(&g_root);

    // 2. YENİ KONUMU BELİRLE VE ÖLÇ (Test modunu kapatman şart!)
    menu_place_root_measured(&g_root, x, y);

    // 3. YENİ MENÜYÜ ÇİZDİR: Yeni konumu damage işaretle ve redraw iste
    desktop_damage_rect(g_root.x, g_root.y, g_root.w, g_root.h);
    desktop_request_redraw();
}

void context_menu_hide(void) {
    if (g_root.visible) {
        g_root.visible = false;
        // Kapandığı yeri temizlemesi için damage bildir
        desktop_damage_rect(g_root.x, g_root.y, g_root.w, g_root.h);
        desktop_request_redraw();
    }
}

bool context_menu_is_visible(void) {
    return g_root.visible;
}

// ------------------------------------------------------------
// DRAW
// ------------------------------------------------------------
static void menu_draw_one(context_menu_t* m) {
    if (!m || !m->visible) return;

    gfx_fill_rect(m->x, m->y, m->w, m->h, 0xCCCCCC);
    gfx_draw_rect(m->x, m->y, m->w, m->h, 0x000000);

    for (int i = 0; i < m->count; i++) {
        int iy = m->y + PAD + i * ITEM_H;

        if (m->hover == i) {
            gfx_fill_rect(m->x + 1, iy, m->w - 2, ITEM_H, 0x000080);
        }

        uint32_t col = (m->hover == i) ? 0xFFFFFF : 0x000000;

        gfx_draw_text_utf8(m->x + PAD + CHECK_W + 6, iy + 5, col, m->items[i].text);

        if (m->items[i].submenu) {
            gfx_draw_text_utf8(m->x + m->w - PAD - ARROW_W, iy + 5, col, ">");
        }
    }
}

void context_menu_draw(void) {
    if (!g_root.visible) return;

#if CONTEXT_MENU_TEST_RECT
    gfx_fill_rect(g_root.x, g_root.y, g_root.w, g_root.h, 0xFFFFFF);
    gfx_draw_rect(g_root.x, g_root.y, g_root.w, g_root.h, 0x000000);
#else
    menu_draw_one(&g_root);
    if (g_root.child && g_root.child->visible) {
        menu_draw_one(g_root.child);
    }
#endif
}

// ------------------------------------------------------------
// INPUT
// ------------------------------------------------------------
void context_menu_handle_mouse(int mx, int my, bool clicked) {
    if (!g_root.visible) return;

#if CONTEXT_MENU_TEST_RECT
    // Test: sadece dışarı tıklayınca kapat
    if (clicked) {
        if (!menu_hit(&g_root, mx, my)) {
            context_menu_hide();
        }
    }
    return;
#else
    context_menu_t* root = &g_root;
    context_menu_t* child = root->child;

    // 1) child üstünde
    if (child && child->visible && menu_hit(child, mx, my)) {
        int idx = menu_item_at(child, mx, my);
        child->hover = idx;

        if (clicked && idx >= 0 && idx < child->count) {
            menu_item_t* it = &child->items[idx];
            if (it->callback) it->callback();
            context_menu_hide();
        }
        return;
    }

    // 2) root
    int ridx = menu_item_at(root, mx, my);
    root->hover = ridx;

    if (ridx >= 0 && ridx < root->count) {
        menu_item_t* it = &root->items[ridx];

        if (it->submenu) {
            if (root->child != it->submenu || !it->submenu->visible) {
                menu_close_child(root);
                root->child = it->submenu;
                root->child->visible = true;
                root->child->parent = root;
                root->child->parent_item_index = ridx;

                menu_place_submenu(root, root->child, ridx);
            }
        } else {
            menu_close_child(root);
        }

        if (clicked) {
            if (!it->submenu) {
                if (it->callback) it->callback();
                context_menu_hide();
            }
        }
        return;
    }

    // 3) dışarı tıkla -> kapat
    if (clicked) {
        if (!(menu_hit(root, mx, my) || (child && child->visible && menu_hit(child, mx, my)))) {
            context_menu_hide();
        }
    } else {
        // hover dışarı çıktıysa child kapat
        menu_close_child(root);
    }
#endif
}