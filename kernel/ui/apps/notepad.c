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
#include <ui/messagebox.h>

#include <kernel/drivers/ata_pio.h>
#include <kernel/block/block.h>

// --- DIŞ BİLDİRİMLER ---
extern char kbd_scancode_to_ascii(uint8_t scancode);
extern int  wm_get_mouse_x(void);
extern int  wm_get_mouse_y(void);

// Menü itemları
static const char* notepad_menu_items[] = { "Ac", "Kaydet", "Farkli Kaydet", "Kapat" };

// DEBUG
static int debug_frame_counter = 0;

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
// YARDIMCI: Direkt Kaydet (aktif tab file_path dolu olmalı)
// ------------------------------------------------------------
static void notepad_direct_save(notepad_t* data) {
    notepad_tab_t* tab = ntab(data);
    printk("[Notepad] DIRECT_SAVE called, file_path='%s'\n", tab ? tab->file_path : "(null)");

    if (!data || !tab || strlen(tab->file_path) == 0) {
        printk("[Notepad] DIRECT_SAVE: Invalid data or empty path\n");
        return;
    }

    // VFS dosyasını güncelle
    vfs_remove(tab->file_path);

    vfs_file_t* f = NULL;
    if (vfs_open(tab->file_path, VFS_O_CREAT | VFS_O_WRONLY, &f) == 1) {
        uint32_t written = 0;
        vfs_write(f, tab->text, tab->cursor, &written);
        vfs_close(f);

        // --- ATA DİSKE FİZİKSEL YAZMA (SYNC) ---
        if (ata_pio_is_ready()) {
            blockdev_t* dev = ata_pio_get_dev();
            if (dev && dev->write) {
                int ok = dev->write(dev, 2000, 1, tab->text); // örnek 1 sektör
                printk("[Notepad] ATA write %s\n", ok ? "SUCCESS" : "FAILED");
            }
        }
        // --------------------------------------

        tab->is_dirty = false;

        // Kaydedildi mesajında nereye kaydettiğini göster
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
// SAVE DIALOG CALLBACK: Kaydet onayı (owner_win_id ile)
// ------------------------------------------------------------
static void notepad_on_save_confirm(const char* filename) {
    int owner = save_dialog_get_owner_win_id();
    notepad_t* data = notepad_from_win_id(owner);
    notepad_tab_t* tab = ntab(data);

    if (!data || !tab) {
        printk("[Notepad] SAVE_CONFIRM: owner=%d not found\n", owner);
        return;
    }

    // /home/desktop/<filename>.txt
    strcpy(tab->file_path, "/home/desktop/");
    strcat(tab->file_path, (filename && filename[0]) ? filename : "adsiz");
    if (strstr(tab->file_path, ".txt") == NULL) strcat(tab->file_path, ".txt");

    notepad_direct_save(data);
    data->menu_open = false;

    // Eğer "save sonrası kapat" modundaysak burada kapat
    if (data->close_after_save) {
        int wid = data->window_id;
        data->close_after_save = false;
        data->close_pending = false;
        data->pending_close_win_id = -1;
        wm_close_window(wid);
    }
}

// ------------------------------------------------------------
// OPEN DIALOG CALLBACK: Dosya aç (aynı pencereye yükle)
// Not: open_dialog.c FULL PATH gönderiyor.
// ------------------------------------------------------------
static void notepad_on_open_confirm(const char* full_path) {
    int owner = open_dialog_get_owner_win_id();
    notepad_t* data = notepad_from_win_id(owner);
    notepad_tab_t* tab = ntab(data);

    if (!data || !tab) {
        printk("[Notepad] OPEN_CONFIRM: owner=%d not found\n", owner);
        return;
    }

    if (!full_path || !full_path[0]) return;

    strncpy(tab->file_path, full_path, 127);
    tab->file_path[127] = '\0';

    uint32_t actual_size = 0;
    int result = vfs_read_all(full_path, (uint8_t*)tab->text, NOTEPAD_MAX_TEXT - 1, &actual_size);

    if (result >= 0) {
        tab->cursor = actual_size;
        tab->text[actual_size] = '\0';
    } else {
        tab->cursor = 0;
        tab->text[0] = '\0';
    }

    tab->is_dirty = false;
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

    // messagebox hala açıkken bekle
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

    // Tek tab ile başlat
    data->tab_count = 1;
    data->active_tab = 0;

    notepad_tab_t* tab = &data->tabs[0];
    memset(tab->text, 0, NOTEPAD_MAX_TEXT);
    memset(tab->file_path, 0, 128);
    tab->cursor = 0;
    tab->is_dirty = false;
}

static void notepad_on_draw(app_t* self) {
    if (!self || !self->user) return;
    notepad_t* data = (notepad_t*)self->user;
    notepad_tab_t* tab = ntab(data);
    if (!tab) return;

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

    ui_rect_t client = wm_get_client_rect(self->win_id);
    int mx = wm_get_mouse_x();
    int my = wm_get_mouse_y();

    // Menü bar
    gfx_fill_rect(client.x, client.y, client.w, 20, 0xCCCCCC);

    char header_text[160];
    const char* display_name = (strlen(tab->file_path) > 0) ? tab->file_path : "Adsiz";
    strcpy(header_text, "Dosya: ");
    strncat(header_text, display_name, sizeof(header_text) - strlen(header_text) - 1);
    if (tab->is_dirty) strncat(header_text, "*", sizeof(header_text) - strlen(header_text) - 1);
    gfx_draw_text(client.x + 120, client.y + 5, 0x444444, header_text);

    int btn_x = client.x + 5, btn_y = client.y + 2, btn_w = 55, btn_h = 16;
    bool is_hover = (mx >= btn_x && mx <= btn_x + btn_w && my >= btn_y && my <= btn_y + btn_h);

    if (data->menu_open) {
        gfx_fill_rect(btn_x, btn_y, btn_w, btn_h, 0xAAAAAA);
    } else if (is_hover) {
        gfx_draw_rect(btn_x, btn_y, btn_w, btn_h, 0xFFFFFF);
    }
    gfx_draw_text(btn_x + 8, btn_y + 3, 0x000000, "Dosya");

    // Yazı alanı
    int text_y = client.y + 20;
    gfx_fill_rect(client.x, text_y, client.w, client.h - 20, 0xFFFFFF);
    gfx_draw_line(client.x, text_y, client.x + client.w, text_y, 0x808080);

    int cx = client.x + 5, cy = text_y + 5;
    char buf[2] = {0, 0};

    for (uint32_t i = 0; i < tab->cursor; i++) {
        buf[0] = tab->text[i];
        if (buf[0] == '\n') {
            cy += 14;
            cx = client.x + 5;
        } else {
            gfx_draw_text(cx, cy, 0x000000, buf);
            cx += 8;
        }
        if (cy > client.y + client.h - 14) break;
    }
    gfx_draw_text(cx, cy, 0x000000, "_");

    // Dropdown
    if (data->menu_open) {
        int m_x = btn_x, m_y = client.y + 20;
        gfx_fill_rect(m_x, m_y, 110, 72, 0xFFFFFF);
        gfx_draw_rect(m_x, m_y, 110, 72, 0x000000);

        for (int i = 0; i < 4; i++) {
            int item_y = m_y + 5 + (i * 16);
            if (mx >= m_x && mx <= m_x + 110 && my >= item_y && my <= item_y + 16) {
                gfx_fill_rect(m_x + 1, item_y, 108, 16, 0x000080);
                gfx_draw_text(m_x + 10, item_y + 2, 0xFFFFFF, notepad_menu_items[i]);
            } else {
                gfx_draw_text(m_x + 10, item_y + 2, 0x000000, notepad_menu_items[i]);
            }
        }
    }
}

static void notepad_on_mouse(app_t* self, int mx, int my, uint8_t buttons, uint8_t extra1, uint8_t extra2) {
    (void)extra1; (void)extra2;

    if (!self || !self->user) return;
    notepad_t* data = (notepad_t*)self->user;

    if (save_dialog_is_active()) return;
    if (open_dialog_is_active()) return;
    if (messagebox_is_visible()) return;
    if (wm_is_any_window_captured()) return;

    if (!(buttons & 1)) return;

    ui_rect_t client = wm_get_client_rect(self->win_id);

    bool file_btn_hit = (mx >= client.x && mx <= client.x + 60 &&
                         my >= client.y && my <= client.y + 20);

    if (file_btn_hit) {
        data->menu_open = !data->menu_open;
        return;
    }

    if (data->menu_open) {
        int m_x = client.x + 5, m_y = client.y + 20;
        int m_w = 110, m_h = 72;

        bool menu_area_hit = (mx >= m_x && mx <= m_x + m_w &&
                              my >= m_y && my <= m_y + m_h);

        if (menu_area_hit) {
            int item = (my - m_y - 5) / 16;
            data->menu_open = false;

            if (item == 0) { // Aç
                open_dialog_show("Dosya Ac", "", self->win_id, notepad_on_open_confirm);
                return;
            }

            if (item == 1) { // Kaydet
                notepad_tab_t* tab = ntab(data);
                if (!tab) return;

                if (strlen(tab->file_path) == 0) {
                    save_dialog_show("Kaydet", "adsiz.txt", 0, self->win_id, notepad_on_save_confirm);
                    return;
                }
                notepad_direct_save(data);
                return;
            }

            if (item == 2) { // Farklı Kaydet
                save_dialog_show("Farkli Kaydet", "adsiz.txt", 0, self->win_id, notepad_on_save_confirm);
                return;
            }

            if (item == 3) { // Kapat
                if (self->v && self->v->on_close_request) self->v->on_close_request(self);
                else wm_close_window(self->win_id);
                return;
            }
        } else {
            data->menu_open = false;
        }
    }
}

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
            changed = true;
        }
    }

    if (changed) tab->is_dirty = true;
}

static void notepad_on_destroy(app_t* self) {
    (void)self;
}

// ------------------------------------------------------------
// Masaüstünden dosya açma: NOTEPAD singleton ise aynı pencereye yükler
// ------------------------------------------------------------
void notepad_open_file(const char* path) {
    app_t* self = appmgr_start_app(3);
    if (!self || !self->user) return;

    notepad_t* data = (notepad_t*)self->user;
    notepad_tab_t* tab = ntab(data);
    if (!tab) return;

    if (!path || !path[0]) return;

    strncpy(tab->file_path, path, 127);
    tab->file_path[127] = '\0';

    uint32_t actual_size = 0;
    int result = vfs_read_all(path, (uint8_t*)tab->text, NOTEPAD_MAX_TEXT - 1, &actual_size);

    if (result >= 0) {
        tab->cursor = actual_size;
        tab->text[actual_size] = '\0';
    } else {
        tab->cursor = 0;
        tab->text[0] = '\0';
    }

    tab->is_dirty = false;
    data->menu_open = false;
    data->close_pending = false;
    data->close_after_save = false;
    data->pending_close_win_id = -1;

    wm_set_active(self->win_id);
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
