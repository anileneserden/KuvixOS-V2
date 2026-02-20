// kernel/ui/apps/setup_wizard.c

#include <ui/apps/setup_wizard.h>

#include <app/app_manager.h>
#include <ui/wm.h>
#include <ui/dialogs/open_dialog.h>
#include <ui/desktop_icons.h>

#include <kernel/drivers/video/gfx.h>
#include <lib/string.h>
#include <stdint.h>
#include <stdbool.h>

#include <kernel/fs/vfs.h>
#include <kernel/printk.h>

extern int wm_get_mouse_x(void);
extern int wm_get_mouse_y(void);

// ------------------------------------------------------------
// Helpers (client-relative hit test)
// ------------------------------------------------------------
static bool hit(int mx, int my, int x, int y, int w, int h) {
    return (mx >= x && mx <= x + w && my >= y && my <= y + h);
}

static void draw_button(int x, int y, int w, int h,
                        const char* text,
                        bool enabled,
                        bool hover)
{
    uint32_t bg = enabled ? 0xDDDDDD : 0xBBBBBB;
    uint32_t bd = hover ? 0xFFFFFF : 0x444444;

    gfx_fill_rect(x, y, w, h, bg);
    gfx_draw_rect(x, y, w, h, bd);
    gfx_draw_text_utf8(x + 10, y + 6, 0x000000, text);
}

static void draw_progress_bar(int x, int y, int w, int h, int pct) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;

    gfx_fill_rect(x, y, w, h, 0xEEEEEE);
    gfx_draw_rect(x, y, w, h, 0x444444);

    int fill = (w - 2) * pct / 100;
    gfx_fill_rect(x + 1, y + 1, fill, h - 2, 0x00AA00);
}

// ------------------------------------------------------------
// Navigation helpers (client-relative layout)
// ------------------------------------------------------------
typedef struct {
    int back_x, back_y, back_w, back_h;
    int next_x, next_y, next_w, next_h;
    int cancel_x, cancel_y, cancel_w, cancel_h;
} nav_ui_t;

static nav_ui_t nav_layout(int client_w, int client_h) {
    nav_ui_t n;

    int bar_h = 34;
    int y = client_h - bar_h;

    int w = 80, h = 24;
    int pad = 8;

    n.cancel_x = pad;
    n.cancel_y = y + 5;
    n.cancel_w = w;
    n.cancel_h = h;

    n.next_x = client_w - pad - w;
    n.next_y = y + 5;
    n.next_w = w;
    n.next_h = h;

    n.back_x = n.next_x - pad - w;
    n.back_y = y + 5;
    n.back_w = w;
    n.back_h = h;

    return n;
}

static void go_next(setup_wizard_t* st) {
    if (st->step == WZ_WELCOME) {
        st->step = WZ_TARGET;
    } else if (st->step == WZ_TARGET) {
        st->step = WZ_LICENSE;
    } else if (st->step == WZ_LICENSE && st->license_accepted) {
        st->step = WZ_PROGRESS;
        st->progress = 0;
        st->tick = 0;
        st->did_install = false;   // ✅ reset
    }
}

static void go_back(setup_wizard_t* st) {
    if (st->step == WZ_TARGET)
        st->step = WZ_WELCOME;
    else if (st->step == WZ_LICENSE)
        st->step = WZ_TARGET;
}

// ------------------------------------------------------------
// owner_win_id -> setup_wizard_t
// ------------------------------------------------------------
static setup_wizard_t* wizard_from_win_id(int win_id) {
    app_t* a = appmgr_get_app_by_window_id(win_id);
    if (!a || !a->user) return NULL;
    return (setup_wizard_t*)a->user;
}

// ------------------------------------------------------------
// path -> target dir çıkar
// ------------------------------------------------------------
static void set_target_dir_from_path(setup_wizard_t* st, const char* full_path) {
    if (!st || !full_path || !full_path[0]) return;

    vfs_stat_t stt;
    int r = vfs_stat(full_path, &stt);
    bool stat_ok = (r == 0 || r == 1);

    if (stat_ok) {
        if (stt.type == VFS_T_DIR || stt.type != VFS_T_FILE) {
            strncpy(st->target_path, full_path, sizeof(st->target_path) - 1);
            st->target_path[sizeof(st->target_path) - 1] = '\0';

            int len = (int)strlen(st->target_path);
            if (len > 1 && st->target_path[len - 1] == '/')
                st->target_path[len - 1] = '\0';
            return;
        }
    }

    // file -> parent
    char tmp[128];
    memset(tmp, 0, sizeof(tmp));
    strncpy(tmp, full_path, sizeof(tmp) - 1);

    int len = (int)strlen(tmp);
    if (len > 1 && tmp[len - 1] == '/') tmp[len - 1] = '\0';

    char* last = strrchr(tmp, '/');
    if (!last) { strcpy(st->target_path, "/"); return; }

    if (last == tmp) tmp[1] = '\0';
    else *last = '\0';

    strncpy(st->target_path, tmp, sizeof(st->target_path) - 1);
    st->target_path[sizeof(st->target_path) - 1] = '\0';
}

// ------------------------------------------------------------
// Open dialog callback
// ------------------------------------------------------------
static void setup_wizard_on_open_confirm(const char* full_path) {
    int owner = open_dialog_get_owner_win_id();
    setup_wizard_t* st = wizard_from_win_id(owner);
    if (!st) return;

    set_target_dir_from_path(st, full_path);

    printk("[WZ] path='%s'\n", full_path);
    vfs_stat_t t;
    int r = vfs_stat(full_path, &t);
    printk("[WZ] stat r=%d type=%d size=%u\n", r, t.type, t.size);
}

// ------------------------------------------------------------
// Install action (runs once)
// ------------------------------------------------------------
static void do_install_once(setup_wizard_t* st) {
    if (!st || st->did_install) return;
    st->did_install = true;

    vfs_mkdir("/home");
    vfs_mkdir("/home/desktop");
    vfs_mkdir("/home/apps");

    vfs_mkdir(st->target_path);

    char appdir[128];
    memset(appdir, 0, sizeof(appdir));
    strncpy(appdir, st->target_path, sizeof(appdir) - 1);

    int l = (int)strlen(appdir);
    if (l > 0 && appdir[l - 1] != '/') {
        if (l < (int)sizeof(appdir) - 1) strcat(appdir, "/");
    }
    if ((int)strlen(appdir) < (int)sizeof(appdir) - 1) strcat(appdir, "demoapp");
    vfs_mkdir(appdir);

    char instfile[160];
    memset(instfile, 0, sizeof(instfile));
    strncpy(instfile, appdir, sizeof(instfile) - 1);
    if ((int)strlen(instfile) < (int)sizeof(instfile) - 1) strcat(instfile, "/installed.txt");

    const char* txt = "KuvixOS Setup Wizard: kurulum basarili!\n";
    vfs_write_all(instfile, (const uint8_t*)txt, strlen(txt));

    // Masaüstüne kısayol (demo)
    if (st->add_desktop_icon) {
        const char* ksf =
            "type=app\n"
            "title=Notepad\n"
            "app_id=3\n";
        vfs_write_all("/home/desktop/Notepad.ksf", (const uint8_t*)ksf, strlen(ksf));
    }

    desktop_icons_init();
    desktop_icons_snap_all();
}

// ------------------------------------------------------------
// App callbacks
// ------------------------------------------------------------
static void setup_wizard_on_create(app_t* self) {
    setup_wizard_t* st = (setup_wizard_t*)self->user;
    if (!st) return;

    st->window_id = self->win_id;
    st->step = WZ_WELCOME;

    memset(st->target_path, 0, sizeof(st->target_path));
    strcpy(st->target_path, "/home/apps");

    st->license_accepted = false;
    st->progress = 0;
    st->tick = 0;
    st->did_install = false;
    st->add_desktop_icon = false;
}

static void setup_wizard_on_draw(app_t* self) {
    if (!self || !self->user) return;

    setup_wizard_t* st = (setup_wizard_t*)self->user;

    // WM origin client'a set -> çizimler 0,0'dan
    ui_rect_t client = wm_get_client_rect(self->win_id);

    // Mouse ekran coords -> client coords
    int mx = wm_get_mouse_x();
    int my = wm_get_mouse_y();
    int lx = mx - client.x;
    int ly = my - client.y;

    // Background
    gfx_fill_rect(0, 0, client.w, client.h, 0xF4F4F4);

    // Content area (nav bar hariç)
    int nav_h = 34;
    ui_rect_t content = (ui_rect_t){ 0, 0, client.w, client.h - nav_h };

    // ------------------------------------------------------------
    // Render step
    // ------------------------------------------------------------
    switch (st->step) {
        case WZ_WELCOME:
            gfx_draw_text_utf8(content.x + 10, content.y + 10, 0x111111,
                          "KuvixOS Kurulum Sihirbazı");
            gfx_draw_text_utf8(content.x + 10, content.y + 32, 0x222222,
                          "Bu uygulama bir demo setup wizard'dır.");
            gfx_draw_text_utf8(content.x + 10, content.y + 46, 0x222222,
                          "İleri'ye basarak devam edebilirsin.");
            break;

        case WZ_TARGET: {
            gfx_draw_text_utf8(content.x + 10, content.y + 10, 0x111111,
                          "Kurulum Yeri Seç");
            gfx_draw_text_utf8(content.x + 10, content.y + 32, 0x222222,
                          "Hedef klasör:");

            int bx = content.x + 10;
            int by = content.y + 52;
            int bh = 22;

            int btn_w = 26;
            int gap = 6;

            int tw = (content.w - 20) - (btn_w + gap);
            int btn_x = bx + tw + gap;
            int btn_y = by;

            // textbox
            gfx_fill_rect(bx, by, tw, bh, 0xFFFFFF);
            gfx_draw_rect(bx, by, tw, bh, 0x444444);
            gfx_draw_text_utf8(bx + 6, by + 6, 0x000000, st->target_path);

            // "..." button
            bool hov = hit(lx, ly, btn_x, btn_y, btn_w, bh);
            gfx_fill_rect(btn_x, btn_y, btn_w, bh, 0xDDDDDD);
            gfx_draw_rect(btn_x, btn_y, btn_w, bh, hov ? 0xFFFFFF : 0x444444);
            gfx_draw_text_utf8(btn_x + 7, btn_y + 6, 0x000000, "...");

            gfx_draw_text_utf8(content.x + 10, content.y + 82, 0x222222,
                          "Butona basıp bir yol seç (klasör seçimi).");

            // checkbox: Masaustune ikon ekle
            int cbx = content.x + 10;
            int cby = content.y + 110;

            gfx_fill_rect(cbx, cby, 14, 14, 0xFFFFFF);
            gfx_draw_rect(cbx, cby, 14, 14, 0x444444);

            if (st->add_desktop_icon) {
                gfx_draw_line(cbx+3, cby+7, cbx+6, cby+10, 0x000000);
                gfx_draw_line(cbx+6, cby+10, cbx+11, cby+3, 0x000000);
            }

            gfx_draw_text_utf8(cbx + 20, cby + 2, 0x222222, "Masaüstüne ikon ekle");
            break;
        }

        case WZ_LICENSE: {
            gfx_draw_text_utf8(content.x + 10, content.y + 10, 0x111111,
                          "Lisans / Bilgilendirme");
            gfx_draw_text_utf8(content.x + 10, content.y + 32, 0x222222,
                          "Lorem ipsum dolor sit amet...");

            int cbx = content.x + 10;
            int cby = content.y + 60;
            gfx_fill_rect(cbx, cby, 14, 14, 0xFFFFFF);
            gfx_draw_rect(cbx, cby, 14, 14, 0x444444);
            if (st->license_accepted) {
                gfx_draw_line(cbx+3, cby+7, cbx+6, cby+10, 0x000000);
                gfx_draw_line(cbx+6, cby+10, cbx+11, cby+3, 0x000000);
            }
            gfx_draw_text_utf8(cbx + 20, cby + 2, 0x222222, "Okudum, kabul ediyorum");
            break;
        }

        case WZ_PROGRESS:
            gfx_draw_text_utf8(content.x + 10, content.y + 10, 0x111111, "Kuruluyor...");
            draw_progress_bar(content.x + 10, content.y + 40, content.w - 20, 18, st->progress);
            break;

        case WZ_DONE:
            gfx_draw_text_utf8(content.x + 10, content.y + 10, 0x111111, "Kurulum Tamamlandı");
            gfx_draw_text_utf8(content.x + 10, content.y + 32, 0x222222, "Demo kurulum bitti.");
            break;
    }

    // ------------------------------------------------------------
    // Fake progress + INSTALL (tek sefer)
    // ------------------------------------------------------------
    if (st->step == WZ_PROGRESS) {
        st->tick++;
        if ((st->tick % 3) == 0) {
            if (st->progress < 100) st->progress++;

            if (st->progress >= 100) {
                if (!st->did_install) do_install_once(st);
                st->step = WZ_DONE;
            }
        }
    }

    // ------------------------------------------------------------
    // Nav bar (client-relative)
    // ------------------------------------------------------------
    nav_ui_t n = nav_layout(client.w, client.h);

    gfx_fill_rect(0, client.h - 34, client.w, 34, 0xCCCCCC);

    bool can_back = (st->step == WZ_TARGET || st->step == WZ_LICENSE);
    bool can_next = true;
    bool can_cancel = (st->step != WZ_PROGRESS);

    if (st->step == WZ_LICENSE && !st->license_accepted) can_next = false;
    if (st->step == WZ_PROGRESS) can_next = false;

    const char* next_text = (st->step == WZ_DONE) ? "Kapat" : "İleri";

    draw_button(n.cancel_x, n.cancel_y, n.cancel_w, n.cancel_h,
                "İptal", can_cancel,
                hit(lx, ly, n.cancel_x, n.cancel_y, n.cancel_w, n.cancel_h));

    draw_button(n.back_x, n.back_y, n.back_w, n.back_h,
                "Geri", can_back,
                hit(lx, ly, n.back_x, n.back_y, n.back_w, n.back_h));

    draw_button(n.next_x, n.next_y, n.next_w, n.next_h,
                next_text, can_next,
                hit(lx, ly, n.next_x, n.next_y, n.next_w, n.next_h));
}

static void setup_wizard_on_mouse(app_t* self,
                                  int mx, int my,
                                  uint8_t buttons,
                                  uint8_t e1, uint8_t e2)
{
    (void)e1; (void)e2;
    if (!self || !self->user) return;

    setup_wizard_t* st = (setup_wizard_t*)self->user;

    // "basılıyken sürekli" tetiklemeyi engelle (latch)
    static uint8_t prev_buttons = 0;
    uint8_t pressed = (uint8_t)(buttons & ~prev_buttons);
    prev_buttons = buttons;

    if (!(pressed & 1)) return;

    ui_rect_t client = wm_get_client_rect(self->win_id);
    int lx = mx - client.x;
    int ly = my - client.y;

    nav_ui_t n = nav_layout(client.w, client.h);

    // TARGET ekranında checkbox + "..."
    if (st->step == WZ_TARGET && !open_dialog_is_active()) {
        int nav_h = 34;
        ui_rect_t content = (ui_rect_t){ 0, 0, client.w, client.h - nav_h };

        int cbx = content.x + 10;
        int cby = content.y + 110;

        if (hit(lx, ly, cbx, cby, 14, 14) ||
            hit(lx, ly, cbx + 20, cby, 220, 14)) {
            st->add_desktop_icon = !st->add_desktop_icon;
            return;
        }

        int bx = content.x + 10;
        int by = content.y + 52;
        int bh = 22;

        int btn_w = 26;
        int gap = 6;
        int tw = (content.w - 20) - (btn_w + gap);
        int btn_x = bx + tw + gap;

        if (hit(lx, ly, btn_x, by, btn_w, bh)) {
            open_dialog_show_dirpicker("Klasor Sec", st->target_path, self->win_id, setup_wizard_on_open_confirm);
            return;
        }
    }

    // LICENSE checkbox
    if (st->step == WZ_LICENSE) {
        int nav_h = 34;
        ui_rect_t content = (ui_rect_t){ 0, 0, client.w, client.h - nav_h };

        int cbx = content.x + 10;
        int cby = content.y + 60;
        if (hit(lx, ly, cbx, cby, 14, 14) ||
            hit(lx, ly, cbx + 20, cby, 200, 14)) {
            st->license_accepted = !st->license_accepted;
            return;
        }
    }

    // Cancel
    if (hit(lx, ly, n.cancel_x, n.cancel_y, n.cancel_w, n.cancel_h) &&
        st->step != WZ_PROGRESS) {
        wm_close_window(self->win_id);
        return;
    }

    // Back
    if (hit(lx, ly, n.back_x, n.back_y, n.back_w, n.back_h)) {
        if (st->step == WZ_TARGET || st->step == WZ_LICENSE)
            go_back(st);
        return;
    }

    // Next / Close
    if (hit(lx, ly, n.next_x, n.next_y, n.next_w, n.next_h)) {
        if (st->step == WZ_DONE) {
            wm_close_window(self->win_id);
            return;
        }
        if (st->step == WZ_LICENSE && !st->license_accepted) return;
        if (st->step == WZ_PROGRESS) return;

        go_next(st);
        return;
    }
}

static void setup_wizard_on_key(app_t* self, uint16_t sc) {
    (void)self; (void)sc;
}

static void setup_wizard_on_destroy(app_t* self) {
    (void)self;
}

static int setup_wizard_on_close_request(app_t* self) {
    setup_wizard_t* st = (setup_wizard_t*)self->user;
    if (!st) return 1;
    if (st->step == WZ_PROGRESS) return 0;
    return 1;
}

const app_vtbl_t setup_wizard_vtbl = {
    .on_create        = setup_wizard_on_create,
    .on_draw          = setup_wizard_on_draw,
    .on_key           = setup_wizard_on_key,
    .on_mouse         = setup_wizard_on_mouse,
    .on_destroy       = setup_wizard_on_destroy,
    .on_close_request = setup_wizard_on_close_request
};