// kernel/ui/apps/notepad.c

#include <app/app.h>
#include <app/app_manager.h>
#include <ui/wm.h>
#include <ui/dialogs/save_dialog.h>
#include <ui/dialogs/open_dialog.h>
#include <kernel/drivers/video/gfx.h>
#include <lib/string.h>
#include <stdint.h>
#include <stdbool.h>
#include <kernel/fs/vfs.h>
#include <ui/notification.h>
#include <ui/apps/notepad.h>
#include <kernel/printk.h>
#include <ui/dialogs/messagebox.h>
#include <kernel/user.h>

// --- DIŞ BİLDİRİMLER ---
extern char kbd_scancode_to_ascii(uint8_t scancode);
extern int  wm_get_mouse_x(void);
extern int  wm_get_mouse_y(void);

// Menü itemları
static const char* notepad_menu_items[] = { "Aç", "Kaydet", "Farklı Kaydet", "Kapat" };

// DEBUG
static int debug_frame_counter = 0;
extern uint32_t g_ticks_ms;

#define MENU_H 20

// ------------------------------------------------------------
// Yardımcı: win_id -> notepad_t
// ------------------------------------------------------------
static notepad_t* notepad_from_win_id(int win_id) {
    app_t* a = appmgr_get_app_by_window_id(win_id);
    if (!a || !a->user) return NULL;
    return (notepad_t*)a->user;
}

// ------------------------------------------------------------
// Yardımcı: aktif tab
// ------------------------------------------------------------
static inline notepad_tab_t* ntab(notepad_t* n) {
    if (!n) return NULL;
    int t = n->active_tab;
    if (t < 0) t = 0;
    if (t >= n->tab_count) t = 0;
    return &n->tabs[t];
}

// ------------------------------------------------------------
// Mouse: screen -> local (content/client origin'e göre)
// Not: WM tarafında gfx_set_origin(content.x, content.y) yapıyorsun.
// Bu yüzden çizimler 0,0'dan; mouse'u da origin'e göre çevirmeliyiz.
// ------------------------------------------------------------
static inline void mouse_to_local(const ui_rect_t* area, int mx, int my, int* out_lx, int* out_ly)
{
    // area ekran koordinatlarında rect: x,y,w,h (content rect)
    if (mx >= area->x && mx < area->x + area->w &&
        my >= area->y && my < area->y + area->h) {
        *out_lx = mx - area->x;
        *out_ly = my - area->y;
    } else {
        // dışarıda ise yine de relative hesapla (negatif olabilir)
        *out_lx = mx - area->x;
        *out_ly = my - area->y;
    }
}

// ------------------------------------------------------------
// DIRECT SAVE
// ------------------------------------------------------------
static void notepad_direct_save(notepad_t* data) {
    notepad_tab_t* tab = ntab(data);
    printk("[Notepad] DIRECT_SAVE called, file_path='%s'\n", tab ? tab->file_path : "(null)");

    if (!data || !tab || strlen(tab->file_path) == 0) {
        printk("[Notepad] DIRECT_SAVE: Invalid data or empty path\n");
        return;
    }

    if (tab->cursor >= NOTEPAD_MAX_TEXT) tab->cursor = NOTEPAD_MAX_TEXT - 1;
    tab->text[tab->cursor] = '\0';

    vfs_remove(tab->file_path);

    vfs_file_t* f = NULL;
    if (vfs_open(tab->file_path, VFS_O_CREAT | VFS_O_WRONLY, &f) == 1) {
        uint32_t written = 0;
        vfs_write(f, tab->text, tab->cursor, &written);
        vfs_close(f);

        printk("[Notepad] saved bytes=%u\n", written);

        tab->is_dirty = false;

        char msg[160];
        memset(msg, 0, sizeof(msg));
        strcpy(msg, "Kaydedildi: ");
        strncat(msg, tab->file_path, sizeof(msg) - strlen(msg) - 1);
        notification_show(msg, 1500);
    } else {
        notification_show("Hata: Kaydedilemedi!", 2000);
    }
}

// ------------------------------------------------------------
// SAVE DIALOG CALLBACK
// ------------------------------------------------------------
static void notepad_on_save_confirm(const char* filename) {
    int owner = save_dialog_get_owner_win_id();
    notepad_t* data = notepad_from_win_id(owner);
    notepad_tab_t* tab = ntab(data);

    if (!data || !tab) {
        printk("[Notepad] SAVE_CONFIRM: owner=%d not found\n", owner);
        return;
    }

    strcpy(tab->file_path, USER_DESKTOP_PATH "/");
    strcat(tab->file_path, (filename && filename[0]) ? filename : "adsiz");
    if (strstr(tab->file_path, ".txt") == NULL) strcat(tab->file_path, ".txt");

    notepad_direct_save(data);
    data->menu_open = false;

    if (data->close_after_save) {
        int wid = data->window_id;
        data->close_after_save = false;
        data->close_pending = false;
        data->pending_close_win_id = -1;
        wm_close_window(wid);
    }
}

// ------------------------------------------------------------
// OPEN DIALOG CALLBACK
// ------------------------------------------------------------
static void notepad_on_open_confirm(const char* full_path) {
    int owner = open_dialog_get_owner_win_id();
    notepad_t* data = notepad_from_win_id(owner);

    if (!data) {
        printk("[Notepad] OPEN_CONFIRM: owner=%d not found\n", owner);
        return;
    }
    if (!full_path || !full_path[0]) return;

    data->pending_open = true;
    strncpy(data->pending_path, full_path, sizeof(data->pending_path) - 1);
    data->pending_path[sizeof(data->pending_path) - 1] = '\0';

    data->menu_open = false;
}

// ------------------------------------------------------------
// CLOSE PROMPT sonucu işleme
// ------------------------------------------------------------
static void notepad_process_close_prompt(app_t* app) {
    if (!app || !app->user) return;

    notepad_t* data = (notepad_t*)app->user;
    notepad_tab_t* tab = ntab(data);
    if (!data || !tab) return;

    if (!data->close_pending) return;
    if (messagebox_is_visible()) return;

    MB_RES_T r = messagebox_get_result();
    if (r == MB_RES_NONE) return;

    messagebox_reset_result();
    data->close_pending = false;

    if (r == MB_RES_YES) {
        if (strlen(tab->file_path) > 0) {
            notepad_direct_save(data);
            wm_close_window(data->pending_close_win_id);
        } else {
            data->close_after_save = true;
            save_dialog_show("Kaydet", "adsiz.txt", 0, app->win_id, notepad_on_save_confirm);
        }
    } else if (r == MB_RES_NO) {
        wm_close_window(data->pending_close_win_id);
    }
}

// ------------------------------------------------------------
// APP CALLBACK'LERİ
// ------------------------------------------------------------
static void notepad_on_create(app_t* self) {
    notepad_t* data = (notepad_t*)self->user;
    if (!data) return;

    data->window_id = self->win_id;
    data->active = true;

    data->menu_open = false;

    data->close_pending = false;
    data->close_after_save = false;
    data->pending_close_win_id = -1;

    data->tab_count = 1;
    data->active_tab = 0;

    data->pending_open = false;
    data->pending_path[0] = '\0';

    data->caret_last_ms  = g_ticks_ms;
    data->caret_blink_ms = 0;
    data->caret_visible  = 1;

    notepad_tab_t* tab = &data->tabs[0];
    memset(tab->text, 0, NOTEPAD_MAX_TEXT);
    memset(tab->file_path, 0, sizeof(tab->file_path));
    tab->cursor = 0;
    tab->is_dirty = false;
}

// ------------------------------------------------------------
// DRAW
// ------------------------------------------------------------
static void notepad_on_draw(app_t* self) {
    if (!self || !self->user) return;
    notepad_t* data = (notepad_t*)self->user;
    notepad_tab_t* tab = ntab(data);
    if (!tab) return;

    // Caret blink (instance-based)
    uint32_t now = g_ticks_ms;
    uint32_t dt  = now - data->caret_last_ms;
    data->caret_last_ms = now;

    if (dt > 2000) dt = 0;

    data->caret_blink_ms += dt;
    if (data->caret_blink_ms >= 500) {
        data->caret_blink_ms %= 500;
        data->caret_visible = !data->caret_visible;
        wm_invalidate();
    }

    // pending open: draw'da oku
    if (data->pending_open) {
        data->pending_open = false;

        const char* p = data->pending_path;
        printk("[Notepad] pending open: '%s'\n", p ? p : "(null)");

        if (p && p[0]) {
            strncpy(tab->file_path, p, sizeof(tab->file_path) - 1);
            tab->file_path[sizeof(tab->file_path) - 1] = '\0';

            uint32_t actual_size = 0;
            int result = vfs_read_all(p, (uint8_t*)tab->text, NOTEPAD_MAX_TEXT - 1, &actual_size);
            printk("[Notepad] read_all: result=%d size=%u\n", result, actual_size);

            if (result >= 0) {
                if (actual_size >= (NOTEPAD_MAX_TEXT - 1)) actual_size = (NOTEPAD_MAX_TEXT - 1);
                tab->cursor = actual_size;
                tab->text[actual_size] = '\0';
            } else {
                tab->cursor = 0;
                tab->text[0] = '\0';
            }

            tab->is_dirty = false;
            data->menu_open = false;
        } else {
            tab->cursor = 0;
            tab->text[0] = '\0';
        }
    }

    debug_frame_counter++;
    if (debug_frame_counter % 60 == 0) {
        printk("[Notepad] draw: menu=%d dirty=%d saveDlg=%d openDlg=%d msgBox=%d\n",
            data->menu_open, tab->is_dirty,
            save_dialog_is_active(),
            open_dialog_is_active(),
            messagebox_is_visible());
    }

    notepad_process_close_prompt(self);
    if (save_dialog_is_active() && data->menu_open) data->menu_open = false;
    if (open_dialog_is_active() && data->menu_open) data->menu_open = false;

    // 🔴 ÖNEMLİ:
    // WM tarafında origin’i CONTENT rect’e set ettiğin için burada 0..w,h çiziyoruz.
    // Ama mouse dönüştürmek için ekran coords’ta content rect lazım.
    // Şimdilik wm_get_client_rect() ekran coords döndürüyor diye varsayıyoruz.
    ui_rect_t content = wm_get_client_rect(self->win_id);

    // Mouse ekran coords -> local
    int mx = wm_get_mouse_x();
    int my = wm_get_mouse_y();
    int lx, ly;
    mouse_to_local(&content, mx, my, &lx, &ly);

    int W = content.w;
    int H = content.h;
    if (W <= 0 || H <= 0) return;

    // ------------------------------------------------------------
    // 1) Menü bar (0..W)
    // ------------------------------------------------------------
    gfx_fill_rect(0, 0, W, MENU_H, 0xCCCCCC);

    const int FILE_BTN_X = 5;
    const int FILE_BTN_Y = 2;
    const int FILE_BTN_W = 55;
    const int FILE_BTN_H = 16;

    char header_text[160];
    const char* display_name = (strlen(tab->file_path) > 0) ? tab->file_path : "Adsiz";
    strcpy(header_text, "Dosya: ");
    strncat(header_text, display_name, sizeof(header_text) - (int)strlen(header_text) - 1);
    if (tab->is_dirty) strncat(header_text, "*", sizeof(header_text) - (int)strlen(header_text) - 1);
    gfx_draw_text_utf8(120, 5, 0x444444, header_text);

    bool is_hover =
        (lx >= FILE_BTN_X && lx < FILE_BTN_X + FILE_BTN_W &&
         ly >= FILE_BTN_Y && ly < FILE_BTN_Y + FILE_BTN_H);

    if (data->menu_open) {
        gfx_fill_rect(FILE_BTN_X, FILE_BTN_Y, FILE_BTN_W, FILE_BTN_H, 0xAAAAAA);
    } else if (is_hover) {
        gfx_draw_rect(FILE_BTN_X, FILE_BTN_Y, FILE_BTN_W, FILE_BTN_H, 0xFFFFFF);
    }
    gfx_draw_text_utf8(FILE_BTN_X + 8, FILE_BTN_Y + 3, 0x000000, "Dosya");

    // ------------------------------------------------------------
    // 2) Yazı alanı (MENU_H..H)
    // ------------------------------------------------------------
    int text_y = MENU_H;
    int text_h = H - MENU_H;
    if (text_h < 0) text_h = 0;

    gfx_fill_rect(0, text_y, W, text_h, 0xFFFFFF);
    gfx_draw_line(0, text_y, W - 1, text_y, 0x808080);

    int cx = 5;
    int cy = text_y + 5;
    char buf[2] = {0, 0};

    for (uint32_t i = 0; i < tab->cursor; i++) {
        buf[0] = tab->text[i];

        if (buf[0] == '\n') {
            cy += 14;
            cx = 5;
        } else {
            gfx_draw_text(cx, cy, 0x000000, buf);
            cx += 8;
        }

        if (cy > (H - 14)) break;
    }

    // caret clamp
    if (cy > (H - 14)) cy = (H - 14);
    if (cy < text_y)   cy = text_y;

    if (data->caret_visible) {
        gfx_fill_rect(cx, cy, 1, 14, 0x000000);
    }

    // ------------------------------------------------------------
    // 3) Dropdown
    // ------------------------------------------------------------
    if (data->menu_open) {
        int m_x = FILE_BTN_X;
        int m_y = MENU_H;
        int m_w = 110;
        int m_h = 72;

        gfx_fill_rect(m_x, m_y, m_w, m_h, 0xFFFFFF);
        gfx_draw_rect(m_x, m_y, m_w, m_h, 0x000000);

        for (int i = 0; i < 4; i++) {
            int item_y = m_y + 5 + (i * 16);

            bool hover =
                (lx >= m_x && lx < m_x + m_w &&
                 ly >= item_y && ly < item_y + 16);

            if (hover) {
                gfx_fill_rect(m_x + 1, item_y, m_w - 2, 16, 0x000080);
                gfx_draw_text_utf8(m_x + 10, item_y + 2, 0xFFFFFF, notepad_menu_items[i]);
            } else {
                gfx_draw_text_utf8(m_x + 10, item_y + 2, 0x000000, notepad_menu_items[i]);
            }
        }
    }
}

// ------------------------------------------------------------
// MOUSE
// ------------------------------------------------------------
static void notepad_on_mouse(app_t* self, int mx, int my,
                            uint8_t buttons, uint8_t extra1, uint8_t extra2)
{
    (void)extra1; (void)extra2;

    if (!self || !self->user) return;
    notepad_t* data = (notepad_t*)self->user;

    if (save_dialog_is_active()) return;
    if (open_dialog_is_active()) return;
    if (messagebox_is_visible()) return;

    if (wm_is_any_window_captured()) {
        int cap = wm_get_captured_window_id();
        if (cap != self->win_id && cap != -1) return;
    }

    // Ekran coords’ta content rect
    ui_rect_t content = wm_get_client_rect(self->win_id);

    int lx, ly;
    mouse_to_local(&content, mx, my, &lx, &ly);

    if (lx < 0 || ly < 0 || lx >= content.w || ly >= content.h) return;

    const int FILE_BTN_X = 5;
    const int FILE_BTN_Y = 2;
    const int FILE_BTN_W = 55;
    const int FILE_BTN_H = 16;

    static uint8_t prev_buttons = 0;
    uint8_t pressed = (uint8_t)(buttons & ~prev_buttons);
    prev_buttons = buttons;

    if (!(pressed & 1)) return; // only left press

    bool file_btn_hit =
        (lx >= FILE_BTN_X && lx < FILE_BTN_X + FILE_BTN_W &&
         ly >= FILE_BTN_Y && ly < FILE_BTN_Y + FILE_BTN_H);

    if (file_btn_hit) {
        data->menu_open = !data->menu_open;
        return;
    }

    if (data->menu_open) {
        int m_x = FILE_BTN_X;
        int m_y = MENU_H;
        int m_w = 110;
        int m_h = 72;

        bool menu_area_hit =
            (lx >= m_x && lx < m_x + m_w &&
             ly >= m_y && ly < m_y + m_h);

        if (menu_area_hit) {
            int item = (ly - m_y - 5) / 16; // 0..3

            data->menu_open = false;

            if (item == 0) {
                open_dialog_show("Dosya Aç", "", self->win_id, notepad_on_open_confirm);
                return;
            }
            if (item == 1) {
                notepad_tab_t* tab = ntab(data);
                if (!tab) return;

                if (strlen(tab->file_path) == 0) {
                    save_dialog_show("Kaydet", "adsiz.txt", 0, self->win_id, notepad_on_save_confirm);
                    return;
                }
                notepad_direct_save(data);
                return;
            }
            if (item == 2) {
                save_dialog_show("Farklı Kaydet", "adsiz.txt", 0, self->win_id, notepad_on_save_confirm);
                return;
            }
            if (item == 3) {
                if (self->v && self->v->on_close_request) self->v->on_close_request(self);
                else wm_close_window(self->win_id);
                return;
            }
        } else {
            data->menu_open = false;
        }
    }
}

// ------------------------------------------------------------
// KEY
// ------------------------------------------------------------
static void notepad_on_key(app_t* self, uint16_t scancode) {
    if (!self || !self->user) return;

    notepad_t* data = (notepad_t*)self->user;
    notepad_tab_t* tab = ntab(data);
    if (!tab) return;

    if (save_dialog_is_active()) return;
    if (open_dialog_is_active()) return;
    if (messagebox_is_visible()) return;

    char c = kbd_scancode_to_ascii((uint8_t)scancode);
    bool changed = false;

    if (scancode == 0x1C) { // Enter
        if (tab->cursor < NOTEPAD_MAX_TEXT - 1) {
            tab->text[tab->cursor++] = '\n';
            tab->text[tab->cursor] = '\0';
            changed = true;
        }
    } else if (c == '\b') {
        if (tab->cursor > 0) {
            tab->text[--tab->cursor] = '\0';
            changed = true;
        }
    } else if (c >= 32 && c <= 126) {
        if (tab->cursor < NOTEPAD_MAX_TEXT - 1) {
            tab->text[tab->cursor++] = c;
            tab->text[tab->cursor] = '\0';
            changed = true;
        }
    }

    if (changed) {
        tab->is_dirty = true;

        data->caret_visible = 1;
        data->caret_blink_ms = 0;
        data->caret_last_ms = g_ticks_ms;
    }
}

static void notepad_on_destroy(app_t* self) {
    (void)self;
}

// ------------------------------------------------------------
// Masaüstünden dosya açma
// ------------------------------------------------------------
void notepad_open_file(const char* path) {
    app_t* self = appmgr_start_app(3);
    if (!self || !self->user) return;

    notepad_t* data = (notepad_t*)self->user;
    if (!path || !path[0]) return;

    data->pending_open = true;
    strncpy(data->pending_path, path, sizeof(data->pending_path) - 1);
    data->pending_path[sizeof(data->pending_path) - 1] = '\0';

    data->menu_open = false;
    data->close_pending = false;
    data->close_after_save = false;
    data->pending_close_win_id = -1;

    wm_set_active(self->win_id);

    printk("[Notepad] open_file queued: '%s'\n", data->pending_path);
}

// ------------------------------------------------------------
// WM close hook
// ------------------------------------------------------------
static int notepad_on_close_request(app_t* self) {
    notepad_t* data = (notepad_t*)self->user;
    notepad_tab_t* tab = ntab(data);
    if (!data || !tab) return 1;

    if (!tab->is_dirty) return 1;

    if (!data->close_pending) {
        data->close_pending = true;
        data->pending_close_win_id = self->win_id;

        messagebox_reset_result();
        MessageBox.Show(
            "Not Defteri",
            "Kaydedilmemis degisiklikler var.\nKaydetmek ister misin?",
            MB_ICON_WARNING,
            MB_BTNS_YESNO
        );
    }

    return 0;
}

// ------------------------------------------------------------
// VTABLE
// ------------------------------------------------------------
const app_vtbl_t notepad_vtbl = {
    .on_create        = notepad_on_create,
    .on_draw          = notepad_on_draw,
    .on_key           = notepad_on_key,
    .on_mouse         = notepad_on_mouse,
    .on_destroy       = notepad_on_destroy,
    .on_close_request = notepad_on_close_request
};