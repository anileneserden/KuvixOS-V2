// kernel/ui/apps/file_manager.c

#include <app/app.h>
#include <app/app_manager.h>
#include <ui/wm.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/fs/vfs.h>
#include <lib/string.h>
#include <kernel/printk.h>
#include <stdint.h>
#include <stdbool.h>

#define SIDEBAR_WIDTH   160
#define ITEM_HEIGHT     28
#define MAX_ITEMS       96
#define NAME_MAX        64
#define PATH_MAX        256

// çift tık eşiği
extern uint32_t g_ticks_ms;
#define DBLCLICK_MS 350

// UTF-8 çiziyorsan bunu kullan (gfx.c içinde var)
// Yoksa gfx_draw_text_utf8 yoksa, aşağıda gfx_draw_text ile değiştir.
extern void gfx_draw_text_utf8(int x, int y, uint32_t color, const char* s);

// ------------------------------------------------------------
// State
// ------------------------------------------------------------
typedef struct {
    char name[NAME_MAX];
    uint32_t size;
    bool is_dir;
} fm_item_t;

typedef struct {
    char cwd[PATH_MAX];
    fm_item_t items[MAX_ITEMS];
    int count;

    int selected;

    // double click state
    uint32_t last_click_ms;
    int last_click_index;

    // sidebar selected
    int sidebar_sel;
} file_mgr_t;

// Sidebar hedefleri (istersen bunları runtime da yaparsın)
typedef struct {
    const char* label_utf8;
    const char* path;
} sidebar_link_t;

static const sidebar_link_t g_sidebar[] = {
    { "Masaüstü",   "/home/desktop" },
    { "Ana Dizin",  "/home" },
    { "Sistem",     "/" },
    { "Çöp Kutusu", "/home/trash" },
};

static const int g_sidebar_count = (int)(sizeof(g_sidebar) / sizeof(g_sidebar[0]));

// ------------------------------------------------------------
// Helpers
// ------------------------------------------------------------
static void fm_set_cwd(file_mgr_t* st, const char* path) {
    if (!st || !path) return;
    strncpy(st->cwd, path, PATH_MAX - 1);
    st->cwd[PATH_MAX - 1] = 0;
}

static const char* fm_basename(const char* path) {
    const char* p = strrchr(path, '/');
    return p ? (p + 1) : path;
}

static void fm_join_path(char* out, int out_sz, const char* a, const char* b) {
    if (!out || out_sz <= 0) return;
    out[0] = 0;

    if (!a || !b) return;

    // a "/" ise "//" olmasın
    if (strcmp(a, "/") == 0) {
        strncpy(out, "/", out_sz - 1);
        out[out_sz - 1] = 0;
        strncat(out, b, out_sz - 1 - (int)strlen(out));
        return;
    }

    strncpy(out, a, out_sz - 1);
    out[out_sz - 1] = 0;

    int len = (int)strlen(out);
    if (len > 0 && out[len - 1] != '/') {
        strncat(out, "/", out_sz - 1 - (int)strlen(out));
    }
    strncat(out, b, out_sz - 1 - (int)strlen(out));
}

static void fm_clear_items(file_mgr_t* st) {
    st->count = 0;
    st->selected = -1;
    st->last_click_ms = 0;
    st->last_click_index = -1;
    memset(st->items, 0, sizeof(st->items));
}

// ------------------------------------------------------------
// VFS List Callback
// ------------------------------------------------------------
static int fm_list_cb(const char* full_path, uint32_t size, void* u) {
    file_mgr_t* st = (file_mgr_t*)u;
    if (!st) return 0;
    if (!full_path || !full_path[0]) return 1;
    if (st->count >= MAX_ITEMS) return 0;

    // root/cwd entrylerini atla
    // vfs_list bazı FS’lerde cwd’i de döndürebiliyor
    if (strcmp(full_path, st->cwd) == 0) return 1;

    const char* name = fm_basename(full_path);
    if (!name || !name[0]) return 1;
    if (name[0] == '.') return 1; // gizlileri şimdilik gösterme

    vfs_stat_t stt;
    bool is_dir = false;
    if (vfs_stat(full_path, &stt) == 1) {
        is_dir = (stt.type == VFS_T_DIR);
        size = stt.size;
    }

    fm_item_t* it = &st->items[st->count++];
    strncpy(it->name, name, NAME_MAX - 1);
    it->name[NAME_MAX - 1] = 0;
    it->size = size;
    it->is_dir = is_dir;

    return 1;
}

static void fm_refresh(app_t* app) {
    if (!app || !app->user) return;
    file_mgr_t* st = (file_mgr_t*)app->user;

    fm_clear_items(st);

    // yoksa oluştur (desktop-vfs-ui mantığına uygun)
    vfs_mkdir("/home");
    vfs_mkdir("/home/desktop");
    vfs_mkdir("/home/trash");

    vfs_list(st->cwd, fm_list_cb, st);
}

// ------------------------------------------------------------
// Hit tests
// ------------------------------------------------------------
static int fm_hit_sidebar(int x, int y) {
    // x,y client-relative
    if (x < 0 || x >= SIDEBAR_WIDTH) return -1;

    int start_y = 8;
    for (int i = 0; i < g_sidebar_count; i++) {
        int iy = start_y + i * ITEM_HEIGHT;
        if (y >= iy && y < iy + ITEM_HEIGHT) return i;
    }
    return -1;
}

static int fm_hit_item(file_mgr_t* st, int x, int y) {
    // x,y client-relative
    if (!st) return -1;
    int list_x = SIDEBAR_WIDTH + 8;
    if (x < list_x) return -1;

    int start_y = 34; // header altı
    if (y < start_y) return -1;

    int idx = (y - start_y) / ITEM_HEIGHT;
    if (idx < 0 || idx >= st->count) return -1;
    return idx;
}

// ------------------------------------------------------------
// App Callbacks
// ------------------------------------------------------------
static void file_mgr_on_create(app_t* app) {
    if (!app || !app->user) return;
    file_mgr_t* st = (file_mgr_t*)app->user;
    memset(st, 0, sizeof(*st));

    fm_set_cwd(st, "/home/desktop");
    st->selected = -1;
    st->sidebar_sel = 0;

    fm_refresh(app);
}

static void file_mgr_draw_row_bg(int x, int y, int w, bool selected) {
    if (selected) {
        gfx_fill_rect(x, y, w, ITEM_HEIGHT, 0xFF0055AA);
    } else {
        // satır alt çizgi
        gfx_fill_rect(x, y + ITEM_HEIGHT - 1, w, 1, 0xFFF2F2F2);
    }
}

static void file_mgr_on_draw(app_t* app) {
    if (!app || !app->user) return;
    file_mgr_t* st = (file_mgr_t*)app->user;

    // Client area (mutlaka WM origin’i client’a set ediyorsa 0,0’dan çiziyoruz)
    ui_rect_t c = wm_get_client_rect(app->win_id);
    (void)c;

    // --- Arka plan ---
    gfx_fill_rect(0, 0, c.w, c.h, 0xFFFFFFFF);

    // --- Sidebar ---
    gfx_fill_rect(0, 0, SIDEBAR_WIDTH, c.h, 0xFFF3F3F3);
    gfx_fill_rect(SIDEBAR_WIDTH - 1, 0, 1, c.h, 0xFFCCCCCC);

    int sy = 8;
    for (int i = 0; i < g_sidebar_count; i++) {
        bool sel = (i == st->sidebar_sel);
        if (sel) {
            gfx_fill_rect(6, sy + i * ITEM_HEIGHT, SIDEBAR_WIDTH - 12, ITEM_HEIGHT, 0xFF0055AA);
            gfx_draw_text_utf8(14, sy + i * ITEM_HEIGHT + 6, 0xFFFFFFFF, g_sidebar[i].label_utf8);
        } else {
            gfx_draw_text_utf8(14, sy + i * ITEM_HEIGHT + 6, 0xFF333333, g_sidebar[i].label_utf8);
        }
    }

    // --- Header ---
    int hx = SIDEBAR_WIDTH + 8;
    gfx_draw_text_utf8(hx, 10, 0xFF777777, "Dosya Adı");
    gfx_draw_text_utf8(hx + 200, 10, 0xFF777777, "Boyut");
    gfx_fill_rect(hx, 28, c.w - hx - 8, 1, 0xFFEEEEEE);
    gfx_fill_rect(200, 100, 25, 25, 0xFF999999);

    // --- CWD göster ---
    gfx_draw_text_utf8(hx, 35, 0xFF999999, st->cwd);

    // --- Items ---
    int start_y = 40;
    int list_w = c.w - hx - 8;
    if (list_w < 10) list_w = 10;

    for (int i = 0; i < st->count; i++) {
        int y = start_y + i * ITEM_HEIGHT;

        bool selected = (i == st->selected);
        file_mgr_draw_row_bg(hx, y, list_w, selected);

        uint32_t name_col = selected ? 0xFFFFFFFF : 0xFF111111;
        uint32_t size_col = selected ? 0xFFE8F1FF : 0xFF666666;

        // dir prefix
        /*char line[NAME_MAX + 8];
        if (st->items[i].is_dir) {
            strcpy(line, "[DIR] ");
            strncat(line, st->items[i].name, sizeof(line) - 1 - (int)strlen(line));
        } else {
            strncpy(line, st->items[i].name, sizeof(line) - 1);
            line[sizeof(line) - 1] = 0;
        }*/

        // gfx_draw_text_utf8(hx + 6, y + 10, name_col, line);

        // size (dir için boş)
        if (!st->items[i].is_dir) {
            // basit KB gösterim
            char sz[32];
            uint32_t kb = (st->items[i].size + 1023) / 1024;
            // "123 KB"
            sz[0] = 0;
            // basit itoa
            char num[16];
            int n = (int)kb, p = 0;
            if (n == 0) num[p++] = '0';
            while (n > 0 && p < 15) { num[p++] = (char)('0' + (n % 10)); n /= 10; }
            num[p] = 0;
            // reverse
            for (int a = 0, b = p - 1; a < b; a++, b--) { char t = num[a]; num[a] = num[b]; num[b] = t; }

            strcpy(sz, num);
            strcat(sz, " KB");
            gfx_draw_text_utf8(hx + 220, y + 6, size_col, sz);
        }
    }
}

static void file_mgr_open_selected(app_t* app, file_mgr_t* st, int idx) {
    if (!app || !st) return;
    if (idx < 0 || idx >= st->count) return;

    char full[PATH_MAX];
    fm_join_path(full, sizeof(full), st->cwd, st->items[idx].name);

    if (st->items[idx].is_dir) {
        fm_set_cwd(st, full);
        fm_refresh(app);
        return;
    }

    // dosya -> AppManager açsın
    appmgr_open_path(full);
}

static void file_mgr_on_mouse(app_t* app, int mx, int my,
                             uint8_t pressed, uint8_t released, uint8_t buttons) {
    (void)released; (void)buttons;
    if (!app || !app->user) return;
    file_mgr_t* st = (file_mgr_t*)app->user;

    // mx,my: ekran coords geliyor olabilir.
    // WM origin client’a set ediyorsa app->on_mouse’a da zaten client-relative vermen ideal.
    // Şu an WM sende raw mx,my yolluyor. Client rect çıkarıp relative’ye çeviriyoruz:
    ui_rect_t cr = wm_get_client_rect(app->win_id);
    int x = mx - cr.x;
    int y = my - cr.y;

    if (!(pressed & 0x01)) return;

    // Sidebar click
    int s = fm_hit_sidebar(x, y);
    if (s != -1) {
        st->sidebar_sel = s;
        fm_set_cwd(st, g_sidebar[s].path);
        fm_refresh(app);
        return;
    }

    // Item click
    int idx = fm_hit_item(st, x, y);
    if (idx == -1) return;

    st->selected = idx;

    // double click
    uint32_t now = g_ticks_ms;
    if (st->last_click_index == idx && (now - st->last_click_ms) < DBLCLICK_MS) {
        st->last_click_index = -1;
        st->last_click_ms = 0;
        file_mgr_open_selected(app, st, idx);
        return;
    }

    st->last_click_index = idx;
    st->last_click_ms = now;
}

static void file_mgr_on_key(app_t* app, uint16_t key) {
    if (!app || !app->user) return;
    file_mgr_t* st = (file_mgr_t*)app->user;

    // F5 refresh (senin key mappingine göre değişebilir)
    // Burada "raw scancode" geliyorsa F5 make scancode: 0x3F (set1)
    uint8_t sc = (uint8_t)(key & 0xFF);

    if (sc == 0x3F) { // F5
        fm_refresh(app);
        return;
    }

    // Enter -> open
    if (sc == 0x1C) { // Enter
        if (st->selected != -1) file_mgr_open_selected(app, st, st->selected);
        return;
    }

    // Backspace -> parent
    if (sc == 0x0E) { // Backspace
        // cwd’den bir seviye yukarı
        if (strcmp(st->cwd, "/") == 0) return;

        char tmp[PATH_MAX];
        strncpy(tmp, st->cwd, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = 0;

        char* last = strrchr(tmp, '/');
        if (last) {
            if (last == tmp) {
                // "/abc" -> "/"
                tmp[1] = 0;
            } else {
                *last = 0;
            }
            fm_set_cwd(st, tmp);
            fm_refresh(app);
        }
        return;
    }
}

// ------------------------------------------------------------
// VTABLE
// ------------------------------------------------------------
const app_vtbl_t file_manager_vtbl = {
    .on_create  = file_mgr_on_create,
    .on_draw    = file_mgr_on_draw,
    .on_mouse   = file_mgr_on_mouse,
    .on_key     = file_mgr_on_key,
    .on_destroy = 0
};