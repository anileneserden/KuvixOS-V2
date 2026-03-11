// ui/apps/file_manager.c

#include <app/app.h>
#include <app/app_manager.h>
#include <ui/wm.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/fs/vfs.h>
#include <lib/string.h>
#include <kernel/printk.h>
#include <stdint.h>
#include <stdbool.h>

#include <kernel/user.h>

#define SIDEBAR_WIDTH   160
#define ITEM_HEIGHT     28
#define MAX_ITEMS       96
#define NAME_MAX        64
#define PATH_MAX        256

extern uint32_t g_ticks_ms;
#define DBLCLICK_MS 350

extern void gfx_draw_text_utf8(int x, int y, uint32_t color, const char* s);

// ------------------------------------------------------------
// Layout constants (tek yerden yönet)
// ------------------------------------------------------------
#define PAD_X          8
#define HEADER_TEXT_Y  10
#define HEADER_LINE_Y  28

#define CWD_Y          35
#define CWD_H          20   // CWD satırına ayırdığımız alan (font 16 + boşluk)

#define LIST_START_Y   (CWD_Y + CWD_H) // 55
// İstersen biraz daha boşluk:
#define LIST_GAP       6
#define LIST_Y0        (LIST_START_Y + LIST_GAP) // 61

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

    uint32_t last_click_ms;
    int last_click_index;

    int sidebar_sel;
} file_mgr_t;

typedef struct {
    const char* label_utf8;
    const char* path;
} sidebar_link_t;

static const sidebar_link_t g_sidebar[] = {
    { "Masaüstü",   USER_DESKTOP_PATH },
    { "Ana Dizin",  USER_HOME_PATH },
    { "Sistem",     "/" },
    { "Çöp Kutusu", USER_TRASH_PATH },
    { "Çıkartılabilir Disk", "/removable" },
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

// ------------------------------------------------------------
// Direct-child filter (VFS list global dönüyorsa UI'ı kurtarır)
// dir="/": sadece "/name" (ikinci slash yok) kabul
// dir="/home/anil": sadece "/home/anil/<name>" (kalan kısımda slash yok) kabul
// ------------------------------------------------------------
static bool fm_is_direct_child_of(const char* dir, const char* full) {
    if (!dir || !full) return false;

    int dlen = (int)strlen(dir);
    if (dlen <= 0) return false;

    // root özel-case
    if (strcmp(dir, "/") == 0) {
        if (full[0] != '/') return false;
        const char* rest = full + 1;
        if (!rest[0]) return false;          // "/" gelmesin
        return (strchr(rest, '/') == 0);     // "/name" -> OK, "/a/b" -> NO
    }

    // full path dir ile başlamalı
    if (strncmp(full, dir, (size_t)dlen) != 0) return false;

    // dir tam eşit olmasın
    if (full[dlen] == '\0') return false;

    // dir'den sonra "/" olmalı
    if (full[dlen] != '/') return false;

    const char* rest = full + dlen + 1; // child adı
    if (!rest[0]) return false;

    // direct child ise rest içinde slash olmaz
    return (strchr(rest, '/') == 0);
}

static void fm_clear_items(file_mgr_t* st) {
    st->count = 0;
    st->selected = -1;
    st->last_click_ms = 0;
    st->last_click_index = -1;
    memset(st->items, 0, sizeof(st->items));
}

static bool fm_removable_ready(void) {
    // vfs_list(cb=NULL) sadece kontrol: dir var mı?
    return (vfs_list("/removable", 0, 0) == 1);
}

// ------------------------------------------------------------
// VFS List Callback
// ------------------------------------------------------------
static int fm_list_cb(const char* full_path, uint32_t size, void* u) {
    file_mgr_t* st = (file_mgr_t*)u;
    if (!st) return 0;
    if (!full_path || !full_path[0]) return 1;
    if (st->count >= MAX_ITEMS) return 0;

    if (!fm_is_direct_child_of(st->cwd, full_path)) return 1;

    if (strcmp(full_path, st->cwd) == 0) return 1;

    const char* name = fm_basename(full_path);
    if (!name || !name[0]) return 1;
    if (name[0] == '.') return 1;

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

    vfs_mkdir("/home");
    vfs_mkdir(USER_HOME_PATH);
    vfs_mkdir(USER_DESKTOP_PATH);
    vfs_mkdir(USER_TRASH_PATH);

    vfs_list(st->cwd, fm_list_cb, st);
}

// ------------------------------------------------------------
// Hit tests
// ------------------------------------------------------------
static int fm_hit_sidebar(int x, int y) {
    if (x < 0 || x >= SIDEBAR_WIDTH) return -1;

    int start_y = 8;
    for (int i = 0; i < g_sidebar_count; i++) {
        int iy = start_y + i * ITEM_HEIGHT;
        if (y >= iy && y < iy + ITEM_HEIGHT) return i;
    }
    return -1;
}

static int fm_hit_item(file_mgr_t* st, int x, int y) {
    if (!st) return -1;
    int list_x = SIDEBAR_WIDTH + PAD_X;
    if (x < list_x) return -1;

    // ✅ draw ile aynı başlangıç
    int start_y = LIST_Y0;
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

    fm_set_cwd(st, USER_DESKTOP_PATH);
    st->selected = -1;
    st->sidebar_sel = 0;

    fm_refresh(app);
}

static void file_mgr_draw_row_bg(int x, int y, int w, bool selected) {
    if (selected) {
        gfx_fill_rect(x, y, w, ITEM_HEIGHT, 0xFF0055AA);
    } else {
        gfx_fill_rect(x, y + ITEM_HEIGHT - 1, w, 1, 0xFFF2F2F2);
    }
}

static void file_mgr_on_draw(app_t* app) {
    if (!app || !app->user) return;
    file_mgr_t* st = (file_mgr_t*)app->user;

    ui_rect_t c = wm_get_client_rect(app->win_id);
    (void)c;

    // --- Background ---
    gfx_fill_rect(0, 0, c.w, c.h, 0xFFFFFFFF);

    // --- Sidebar ---
    gfx_fill_rect(0, 0, SIDEBAR_WIDTH, c.h, 0xFFF3F3F3);
    gfx_fill_rect(SIDEBAR_WIDTH - 1, 0, 1, c.h, 0xFFCCCCCC);

    int sy = 8;
    bool removable_ok = fm_removable_ready();

    for (int i = 0; i < g_sidebar_count; i++) {
        bool sel = (i == st->sidebar_sel);
        int iy = sy + i * ITEM_HEIGHT;

        // ✅ sadece removable item disable olsun
        bool is_rem = (strcmp(g_sidebar[i].path, "/removable") == 0);
        bool enabled = (!is_rem) || removable_ok;

        if (sel) {
            // seçili ama disabled ise farklı gösterebiliriz:
            uint32_t bg = enabled ? 0xFF0055AA : 0xFFAAAAAA;
            uint32_t fg = 0xFFFFFFFF;

            gfx_fill_rect(6, iy, SIDEBAR_WIDTH - 12, ITEM_HEIGHT, bg);
            gfx_draw_text_utf8(14, iy + 6, fg, g_sidebar[i].label_utf8);
        } else {
            uint32_t fg = enabled ? 0xFF333333 : 0xFF999999; // ✅ gri
            gfx_draw_text_utf8(14, iy + 6, fg, g_sidebar[i].label_utf8);
        }
    }

    int hx = SIDEBAR_WIDTH + PAD_X;

    // --- Header ---
    gfx_draw_text_utf8(hx, HEADER_TEXT_Y, 0xFF777777, "Dosya Adı");
    gfx_draw_text_utf8(hx + 200, HEADER_TEXT_Y, 0xFF777777, "Boyut");
    gfx_fill_rect(hx, HEADER_LINE_Y, c.w - hx - PAD_X, 1, 0xFFEEEEEE);

    // --- CWD area (ayrı satır olarak) ---
    gfx_fill_rect(hx, CWD_Y - 2, c.w - hx - PAD_X, CWD_H, 0xFFFFFFFF); // temiz alan
    gfx_draw_text_utf8(hx, CWD_Y, 0xFF999999, st->cwd);
    gfx_fill_rect(hx, LIST_START_Y, c.w - hx - PAD_X, 1, 0xFFF0F0F0); // cwd alt çizgi

    // --- Items ---
    int start_y = LIST_Y0;
    int list_w = c.w - hx - PAD_X;
    if (list_w < 10) list_w = 10;

    for (int i = 0; i < st->count; i++) {
        int y = start_y + i * ITEM_HEIGHT;

        bool selected = (i == st->selected);
        file_mgr_draw_row_bg(hx, y, list_w, selected);

        uint32_t name_col = selected ? 0xFFFFFFFF : 0xFF111111;
        uint32_t size_col = selected ? 0xFFE8F1FF : 0xFF666666;

        // ✅ İsim çizimi (sen comment etmişsin, geri açıyoruz)
        char line[NAME_MAX + 8];
        if (st->items[i].is_dir) {
            strcpy(line, "[DIR] ");
            strncat(line, st->items[i].name, sizeof(line) - 1 - (int)strlen(line));
        } else {
            strncpy(line, st->items[i].name, sizeof(line) - 1);
            line[sizeof(line) - 1] = 0;
        }
        gfx_draw_text_utf8(hx + 6, y + 6, name_col, line);

        if (!st->items[i].is_dir) {
            char sz[32];
            uint32_t kb = (st->items[i].size + 1023) / 1024;

            // itoa(kb)
            char num[16];
            int n = (int)kb, p = 0;
            if (n == 0) num[p++] = '0';
            while (n > 0 && p < 15) { num[p++] = (char)('0' + (n % 10)); n /= 10; }
            num[p] = 0;
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

    appmgr_open_path(full);
}

static void file_mgr_on_mouse(app_t* app, int mx, int my,
                             uint8_t pressed, uint8_t released, uint8_t buttons) {
    (void)released; (void)buttons;
    if (!app || !app->user) return;
    file_mgr_t* st = (file_mgr_t*)app->user;

    int x = mx;
    int y = my;

    if (!(pressed & 0x01)) return;

    int s = fm_hit_sidebar(x, y);
    if (s != -1) {
        // ✅ removable disable ise tıklama yok
        bool is_rem = (strcmp(g_sidebar[s].path, "/removable") == 0);
        if (is_rem && !fm_removable_ready()) {
            printk("[FileMgr] removable not present\n");
            return;
        }

        st->sidebar_sel = s;
        fm_set_cwd(st, g_sidebar[s].path);
        fm_refresh(app);
        return;
    }

    int idx = fm_hit_item(st, x, y);
    if (idx == -1) return;

    st->selected = idx;

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

    uint8_t sc = (uint8_t)(key & 0xFF);

    if (sc == 0x3F) { fm_refresh(app); return; }     // F5
    if (sc == 0x1C) {                                // Enter
        if (st->selected != -1) file_mgr_open_selected(app, st, st->selected);
        return;
    }

    if (sc == 0x0E) {                                // Backspace -> parent
        if (strcmp(st->cwd, "/") == 0) return;

        char tmp[PATH_MAX];
        strncpy(tmp, st->cwd, sizeof(tmp) - 1);
        tmp[sizeof(tmp) - 1] = 0;

        char* last = strrchr(tmp, '/');
        if (last) {
            if (last == tmp) tmp[1] = 0;
            else *last = 0;
            fm_set_cwd(st, tmp);
            fm_refresh(app);
        }
        return;
    }
}

const app_vtbl_t file_manager_vtbl = {
    .on_create  = file_mgr_on_create,
    .on_draw    = file_mgr_on_draw,
    .on_mouse   = file_mgr_on_mouse,
    .on_key     = file_mgr_on_key,
    .on_destroy = 0
};