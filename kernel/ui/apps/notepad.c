// kernel/ui/apps/notepad.c

#include <app/app.h>
#include <app/app_manager.h>
#include <ui/wm.h>
#include <ui/dialogs/save_dialog.h>
#include <ui/dialogs/open_dialog.h>
#include <ui/dialogs/messagebox.h>
#include <ui/desktop.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/fs/vfs.h>
#include <kernel/printk.h>
#include <kernel/drivers/input/keyboard.h>
#include <kernel/memory/kmalloc.h>
#include <ui/notification.h>
#include <ui/apps/notepad.h>
#include <kernel/user.h>
#include <lib/string.h>
#include <stdint.h>
#include <stdbool.h>
#include <ui/wm.h>

// Menü itemları
static const char* notepad_menu_items[] = { "Aç", "Kaydet", "Farklı Kaydet", "Kapat" };

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
// MINI EDITOR HELPERS (smart edit + selection)
// ------------------------------------------------------------
static inline int np_len(const notepad_tab_t* tab) {
    if (!tab) return 0;
    return (int)tab->len;
}

static inline void np_sel_clear(notepad_tab_t* t) {
    if (!t) return;
    t->sel_active = 0;
    t->sel_anchor = t->cursor;
    t->sel_end    = t->cursor;
}

static inline int np_sel_has(const notepad_tab_t* t) {
    return t && t->sel_active && (t->sel_anchor != t->sel_end);
}

static inline void np_sel_range(const notepad_tab_t* t, uint32_t* a, uint32_t* b) {
    uint32_t s = t->sel_anchor, e = t->sel_end;
    if (s <= e) { *a = s; *b = e; }
    else        { *a = e; *b = s; }
}

static void np_delete_selection(notepad_tab_t* t) {
    if (!np_sel_has(t)) return;

    uint32_t a, b;
    np_sel_range(t, &a, &b);

    uint32_t len = (uint32_t)strlen(t->text);
    if (a > len) a = len;
    if (b > len) b = len;

    memmove(t->text + a, t->text + b, (len - b) + 1); // +1 => '\0' dahil
    t->cursor = a;
    np_sel_clear(t);
    t->is_dirty = true;
}

static bool np_insert_str(notepad_tab_t* tab, const char* s, int n) {
    if (!tab || !s || n <= 0) return false;

    // text yoksa ilk allocate etmeyi dene
    if (!tab->text || tab->cap < 2) {
        uint32_t cap = (tab->cap >= 2) ? tab->cap : (uint32_t)NOTEPAD_INIT_CAP;
        if (cap < 2) cap = 2;

        tab->text = (char*)kmalloc(cap);
        if (!tab->text) {
            tab->cap = 0;
            tab->len = 0;
            tab->cursor = 0;
            return false;
        }
        tab->cap = cap;
        tab->len = 0;
        tab->cursor = 0;
        tab->text[0] = '\0';
    }

    uint32_t len = tab->len;

    // cursor clamp
    if ((int)tab->cursor < 0) tab->cursor = 0;
    if (tab->cursor > len) tab->cursor = len;

    // ihtiyaç duyulan toplam: len + n + 1 ('\0')
    uint32_t need = len + (uint32_t)n + 1;

    // kapasite yetmiyorsa büyüt
    if (need > tab->cap) {
        uint32_t newcap = tab->cap;

        // 2x büyüt (need'i geçene kadar)
        while (newcap < need) {
            uint32_t next = newcap * 2;
            if (next <= newcap) { // overflow guard
                newcap = need;
                break;
            }
            newcap = next;
        }

        // üst sınır
        if (newcap > (uint32_t)NOTEPAD_MAX_CAP) {
            if (need > (uint32_t)NOTEPAD_MAX_CAP) return false;
            newcap = (uint32_t)NOTEPAD_MAX_CAP;
        }

        char* nb = (char*)kmalloc(newcap);
        if (!nb) return false;

        // eski içeriği kopyala (len + '\0')
        memcpy(nb, tab->text, (size_t)(len + 1));
        kfree(tab->text);

        tab->text = nb;
        tab->cap = newcap;
    }

    // shift right: cursor'dan itibaren tail'i sağa kaydır ( '\0' dahil )
    uint32_t cur = tab->cursor;
    memmove(tab->text + cur + (uint32_t)n,
            tab->text + cur,
            (size_t)(len - cur + 1)); // +1 => '\0' da taşınır

    // insert payload
    memcpy(tab->text + cur, s, (size_t)n);

    tab->cursor = cur + (uint32_t)n;
    tab->len = len + (uint32_t)n;
    tab->text[tab->len] = '\0';

    return true;
}

static bool np_insert_char(notepad_tab_t* tab, char c) {
    return np_insert_str(tab, &c, 1);
}

static bool np_backspace(notepad_tab_t* tab) {
    if (!tab) return false;

    // selection varsa: backspace => selection sil
    if (np_sel_has(tab)) {
        np_delete_selection(tab);
        return true;
    }

    int len = np_len(tab);
    if (tab->cursor == 0) return false;
    if ((int)tab->cursor > len) tab->cursor = (uint32_t)len;

    // delete char before cursor
    memmove(tab->text + tab->cursor - 1,
            tab->text + tab->cursor,
            (size_t)(len - (int)tab->cursor + 1));

    tab->cursor--;
    return true;
}

static bool np_delete_forward(notepad_tab_t* tab) {
    if (!tab) return false;

    // selection varsa: delete => selection sil
    if (np_sel_has(tab)) {
        np_delete_selection(tab);
        return true;
    }

    uint32_t len = (uint32_t)strlen(tab->text);
    if (tab->cursor >= len) return false; // sağda karakter yok

    // cursor’daki karakteri sil (sağdaki)
    memmove(tab->text + tab->cursor,
            tab->text + tab->cursor + 1,
            (len - tab->cursor)); // '\0' dahil taşınmış olur

    return true;
}

static int np_line_start(const notepad_tab_t* tab, int pos) {
    if (!tab) return 0;
    if (pos < 0) pos = 0;
    int len = np_len(tab);
    if (pos > len) pos = len;
    while (pos > 0 && tab->text[pos - 1] != '\n') pos--;
    return pos;
}

static int np_count_leading_spaces(const notepad_tab_t* tab, int line_start) {
    if (!tab) return 0;
    int len = np_len(tab);
    int i = line_start;
    int c = 0;
    while (i < len) {
        char ch = tab->text[i];
        if (ch == ' ') { c++; i++; continue; }
        break;
    }
    return c;
}

static bool np_is_pair_braces(const notepad_tab_t* tab) {
    if (!tab) return false;
    int len = np_len(tab);
    int cur = (int)tab->cursor;
    if (cur <= 0 || cur >= len) return false;
    return (tab->text[cur - 1] == '{' && tab->text[cur] == '}');
}

static bool np_tab4(notepad_tab_t* tab) {
    if (!tab) return false;
    if (np_sel_has(tab)) np_delete_selection(tab);
    return np_insert_str(tab, "    ", 4);
}

static bool np_open_brace_pair(notepad_tab_t* tab) {
    if (!tab) return false;
    if (np_sel_has(tab)) np_delete_selection(tab);

    // "{}" ekle, cursor'u araya al
    if (!np_insert_str(tab, "{}", 2)) return false;
    if (tab->cursor > 0) tab->cursor -= 1;
    return true;
}

static bool np_open_bracket_pair(notepad_tab_t* tab) {
    if (!tab) return false;
    if (np_sel_has(tab)) np_delete_selection(tab);

    if (!np_insert_str(tab, "[]", 2)) return false;
    if (tab->cursor > 0) tab->cursor -= 1;
    return true;
}

static bool np_smart_enter(notepad_tab_t* tab) {
    if (!tab) return false;
    if (np_sel_has(tab)) np_delete_selection(tab);

    int len = np_len(tab);
    if ((int)tab->cursor > len) tab->cursor = (uint32_t)len;

    int ls = np_line_start(tab, (int)tab->cursor);
    int base_indent = np_count_leading_spaces(tab, ls);

    // "{|}" özel durumu
    if (np_is_pair_braces(tab)) {
        char tmp[128];
        int p = 0;

        tmp[p++] = '\n';

        int inner = base_indent + 4;
        for (int i = 0; i < inner && p < (int)sizeof(tmp) - 1; i++) tmp[p++] = ' ';

        tmp[p++] = '\n';

        for (int i = 0; i < base_indent && p < (int)sizeof(tmp) - 1; i++) tmp[p++] = ' ';

        if (!np_insert_str(tab, tmp, p)) return false;

        // Cursor şu an kapanış satırı indentinin sonunda.
        // Cursor'u 1 satır yukarı (inner satırı) indente çek:
        int back = base_indent + 1;
        if (tab->cursor >= (uint32_t)back) tab->cursor -= (uint32_t)back;

        return true;
    }

    // normal enter: mevcut indent’i kopyala
    char tmp[96];
    int p = 0;
    tmp[p++] = '\n';
    for (int i = 0; i < base_indent && p < (int)sizeof(tmp) - 1; i++) tmp[p++] = ' ';
    return np_insert_str(tab, tmp, p);
}

static void np_move_cursor_lr(notepad_tab_t* t, int dir /*-1 left, +1 right*/) {
    if (!t) return;

    uint32_t len = (uint32_t)strlen(t->text);

    if (dir < 0) {
        if (t->cursor > 0) t->cursor--;
    } else {
        if (t->cursor < len) t->cursor++;
    }
}

// selection varken shift yoksa: cursor’u selection baş/sona topla
static void np_collapse_selection(notepad_tab_t* t, int dir /*-1 left, +1 right*/) {
    if (!np_sel_has(t)) return;
    uint32_t a, b;
    np_sel_range(t, &a, &b);
    t->cursor = (dir < 0) ? a : b;
    np_sel_clear(t);
}

static bool np_try_fold_4spaces_to_tab(notepad_tab_t* t) {
    if (!t) return false;
    if (t->cursor < 4) return false;

    uint32_t p = t->cursor;
    if (t->text[p-1]==' ' && t->text[p-2]==' ' && t->text[p-3]==' ' && t->text[p-4]==' ') {
        // 4 space -> 1 tab
        t->text[p-4] = '\t';

        // tail'i 3 sola çek: (p .. end) -> (p-3 ..)
        size_t tail = strlen(t->text + p) + 1; // '\0' dahil
        memmove(t->text + (p - 3), t->text + p, tail);

        t->cursor -= 3;
        return true;
    }
    return false;
}

static int np_line_end(const notepad_tab_t* t, int pos) {
    int len = np_len(t);
    if (pos < 0) pos = 0;
    if (pos > len) pos = len;
    while (pos < len && t->text[pos] != '\n') pos++;
    return pos;
}

// pos’un bulunduğu satır içinde kaç “kolon” (tab'ı 4 say)
static int np_visual_col_from_ls(const notepad_tab_t* t, int line_start, int pos) {
    int col = 0;
    if (!t) return 0;
    int len = np_len(t);
    if (pos > len) pos = len;
    for (int i = line_start; i < pos; i++) {
        char ch = t->text[i];
        if (ch == '\t') col += 4;
        else col += 1;
    }
    return col;
}

// hedef satırda istenen kolona en yakın index’i bul (tab'ı 4 say)
static int np_index_at_visual_col(const notepad_tab_t* t, int line_start, int target_col) {
    if (!t) return line_start;
    int len = np_len(t);
    int i = line_start;
    int col = 0;

    while (i < len && t->text[i] != '\n') {
        int w = (t->text[i] == '\t') ? 4 : 1;
        if (col + w > target_col) break;
        col += w;
        i++;
    }
    return i;
}

static void np_move_cursor_up(notepad_tab_t* t) {
    if (!t) return;
    int len = np_len(t);
    int cur = (int)t->cursor;
    if (cur < 0) cur = 0;
    if (cur > len) cur = len;

    int ls = np_line_start(t, cur);
    if (ls == 0) return; // zaten ilk satır

    int prev_end = ls - 1;                // '\n' karakteri
    int prev_ls  = np_line_start(t, prev_end);

    int col = np_visual_col_from_ls(t, ls, cur);
    int new_pos = np_index_at_visual_col(t, prev_ls, col);

    t->cursor = (uint32_t)new_pos;
}

static void np_move_cursor_down(notepad_tab_t* t) {
    if (!t) return;
    int len = np_len(t);
    int cur = (int)t->cursor;
    if (cur < 0) cur = 0;
    if (cur > len) cur = len;

    int ls = np_line_start(t, cur);
    int le = np_line_end(t, cur);
    if (le >= len) return; // zaten son satır (newline yok)

    int next_ls = le + 1;

    int col = np_visual_col_from_ls(t, ls, cur);
    int new_pos = np_index_at_visual_col(t, next_ls, col);

    t->cursor = (uint32_t)new_pos;
}

// ------------------------------------------------------------
// DIRECT SAVE (cursor'a göre kırpma YOK!)
// ------------------------------------------------------------
static void notepad_direct_save(notepad_t* data) {
    notepad_tab_t* tab = ntab(data);
    printk("[Notepad] DIRECT_SAVE called, file_path='%s'\n", tab ? tab->file_path : "(null)");

    if (!data || !tab || strlen(tab->file_path) == 0) {
        printk("[Notepad] DIRECT_SAVE: Invalid data or empty path\n");
        return;
    }

    // dosyayı sıfırlamak için remove (şimdilik kalsın)
    vfs_remove(tab->file_path);

    vfs_file_t* f = NULL;
    if (vfs_open(tab->file_path, VFS_O_CREAT | VFS_O_WRONLY, &f) == 1) {
        uint32_t written = 0;

        int len = (int)strlen(tab->text);
        if (len < 0) len = 0;
        if (len > NOTEPAD_MAX_TEXT) len = NOTEPAD_MAX_TEXT;

        vfs_write(f, tab->text, (uint32_t)len, &written);
        vfs_close(f);

        printk("[Notepad] saved bytes=%u\n", written);

        tab->is_dirty = false;

        char msg[160];
        memset(msg, 0, sizeof(msg));
        strcpy(msg, "Kaydedildi: ");
        strncat(msg, tab->file_path, sizeof(msg) - (int)strlen(msg) - 1);
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

    // ✅ create/draw race yok: pending ile draw’da okunacak
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

    data->menu_hover_item = -1;
    data->last_lx = -999;
    data->last_ly = -999;

    // Güvenlik: tüm tab'leri sıfırla (ileride free/cleanup yazarken işini kolaylaştırır)
    for (int i = 0; i < NOTEPAD_MAX_TABS; i++) {
        data->tabs[i].text = NULL;
        data->tabs[i].len = 0;
        data->tabs[i].cap = 0;
        data->tabs[i].cursor = 0;
        data->tabs[i].is_dirty = false;
        data->tabs[i].sel_active = 0;
        data->tabs[i].sel_anchor = 0;
        data->tabs[i].sel_end = 0;
        memset(data->tabs[i].file_path, 0, sizeof(data->tabs[i].file_path));
    }

    notepad_tab_t* tab = &data->tabs[0];

    tab->cap    = NOTEPAD_INIT_CAP;
    tab->len    = 0;
    tab->cursor = 0;
    tab->is_dirty = false;

    tab->sel_active = 0;
    tab->sel_anchor = 0;
    tab->sel_end    = 0;

    // cap en az 2 olsun ki '\0' garanti
    if (tab->cap < 2) tab->cap = 2;

    tab->text = (char*)kmalloc(tab->cap);
    if (!tab->text) {
        tab->cap = 0;
        tab->len = 0;
        return;
    }
    tab->text[0] = '\0';
}

// ------------------------------------------------------------
// DRAW
// ------------------------------------------------------------
static void notepad_on_draw(app_t* self) {
    if (!self || !self->user) return;
    notepad_t* data = (notepad_t*)self->user;
    notepad_tab_t* tab = ntab(data);
    printk("[NP] draw len=%u\n", (unsigned)strlen(tab->text));
    if (!tab) return;

    // ✅ Desktop/OpenDialog open race fix: dosyayı create bittikten sonra burada oku
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
                tab->text[actual_size] = '\0';
            } else {
                tab->text[0] = '\0';
            }

            // cursor + selection reset
            tab->cursor = (uint32_t)strlen(tab->text);
            np_sel_clear(tab);

            tab->is_dirty = false;
            data->menu_open = false;
        } else {
            tab->cursor = 0;
            tab->text[0] = '\0';
            np_sel_clear(tab);
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

    ui_rect_t client = wm_get_client_rect(self->win_id);

    // Mouse ekran coords -> client-relative
    int mx = wm_get_mouse_x();
    int my = wm_get_mouse_y();
    int lx = mx - client.x;
    int ly = my - client.y;

    // ------------------------------------------------------------
    // Menü bar (client-relative)
    // ------------------------------------------------------------
    gfx_fill_rect(0, 0, client.w, 20, 0xCCCCCC);

    char header_text[160];
    const char* display_name = (strlen(tab->file_path) > 0) ? tab->file_path : "Adsiz";
    strcpy(header_text, "Dosya: ");
    strncat(header_text, display_name, sizeof(header_text) - (int)strlen(header_text) - 1);
    if (tab->is_dirty) strncat(header_text, "*", sizeof(header_text) - (int)strlen(header_text) - 1);
    gfx_draw_text_utf8(120, 5, 0x444444, header_text);

    int btn_x = 5, btn_y = 2, btn_w = 55, btn_h = 16;
    bool is_hover_btn = (lx >= btn_x && lx <= btn_x + btn_w && ly >= btn_y && ly <= btn_y + btn_h);

    if (data->menu_open) {
        gfx_fill_rect(btn_x, btn_y, btn_w, btn_h, 0xAAAAAA);
    } else if (is_hover_btn) {
        gfx_draw_rect(btn_x, btn_y, btn_w, btn_h, 0xFFFFFF);
    }
    gfx_draw_text_utf8(btn_x + 8, btn_y + 3, 0x000000, "Dosya");

    // ------------------------------------------------------------
    // Yazı alanı
    // ------------------------------------------------------------
    int text_y = 20;
    gfx_fill_rect(0, text_y, client.w, client.h - 20, 0xFFFFFF);
    gfx_draw_line(0, text_y, client.w, text_y, 0x808080);

    // Selection range
    uint32_t sa = 0, sb = 0;
    int has_sel = np_sel_has(tab);
    if (has_sel) np_sel_range(tab, &sa, &sb);

    // Metni çiz + cursor konumunu hesapla
    const int CHAR_W = 8;
    const int CHAR_H = 14;

    const int TAB_SPACES = 4;
    const int TAB_W = CHAR_W * TAB_SPACES;

    int cx = 5, cy = text_y + 5;
    int cur_x = cx, cur_y = cy;

    int len = (int)strlen(tab->text);
    int cur = (int)tab->cursor;
    if (cur < 0) cur = 0;
    if (cur > len) cur = len;

    for (int i = 0; i < len; i++) {
        if (i == cur) { cur_x = cx; cur_y = cy; }

        char ch = tab->text[i];
        if (ch == '\n') {
            cy += CHAR_H;
            cx = 5;
        } else if (ch == '\t') {
            if (has_sel && (uint32_t)i >= sa && (uint32_t)i < sb) {
                gfx_draw_alpha_rect(TAB_W, CHAR_H, 0, 85, 170, 120, cx, cy);
            }
            cx += TAB_W;
        } else {
            if (has_sel && (uint32_t)i >= sa && (uint32_t)i < sb) {
                gfx_draw_alpha_rect(CHAR_W, CHAR_H, 0, 85, 170, 120, cx, cy);
            }

            char buf[2] = { ch, 0 };
            gfx_draw_text(cx, cy, 0x000000, buf);
            cx += CHAR_W;
        }

        if (cy > client.h - CHAR_H) break;
    }

    if (cur == len) { cur_x = cx; cur_y = cy; }
    gfx_draw_text(cur_x, cur_y, 0x000000, "_");

    // ------------------------------------------------------------
    // Dropdown (client-relative)
    // ------------------------------------------------------------
    if (data->menu_open) {
        // hover state update (mouse event olmadan da)
        if (lx != data->last_lx || ly != data->last_ly) {
            data->last_lx = lx;
            data->last_ly = ly;

            int new_hover = -1;
            int m_x = 5,  m_y = 20;
            int m_w = 110, m_h = 72;
            desktop_damage_rect(client.x + m_x, client.y + m_y, m_w, m_h);
            desktop_request_redraw();

            if (lx >= m_x && lx <= m_x + m_w &&
                ly >= m_y && ly <= m_y + m_h) {
                int item = (ly - m_y - 5) / 16;
                if (item >= 0 && item < 4) new_hover = item;
            }

            if (new_hover != data->menu_hover_item) {
                data->menu_hover_item = new_hover;
                desktop_damage_rect(client.x + m_x, client.y + m_y, m_w, m_h);
                desktop_request_redraw();   // ✅ bunu ekle
            }
        }

        // draw dropdown
        int m_x = 5, m_y = 20;
        gfx_fill_rect(m_x, m_y, 110, 72, 0xFFFFFF);
        gfx_draw_rect(m_x, m_y, 110, 72, 0x000000);

        for (int i = 0; i < 4; i++) {
            int item_y = m_y + 5 + (i * 16);

            if (data->menu_hover_item == i) {
                gfx_fill_rect(m_x + 1, item_y, 108, 16, 0x000080);
                gfx_draw_text_utf8(m_x + 10, item_y + 2, 0xFFFFFF, notepad_menu_items[i]);
            } else {
                gfx_draw_text_utf8(m_x + 10, item_y + 2, 0x000000, notepad_menu_items[i]);
            }
        }
    } else {
        data->menu_hover_item = -1;
    }
}

// ------------------------------------------------------------
// MOUSE  (VTBL imzasına uygun: pr/rel/btn)
// ------------------------------------------------------------
static void notepad_on_mouse(app_t* self, int mx, int my,
                            uint8_t pr, uint8_t rel, uint8_t btn) {
    (void)rel;

    if (!self || !self->user) return;
    notepad_t* data = (notepad_t*)self->user;

    if (save_dialog_is_active()) return;
    if (open_dialog_is_active()) return;
    if (messagebox_is_visible()) return;
    // if (wm_is_any_window_captured()) return;

    ui_rect_t client = wm_get_client_rect(self->win_id);
    int lx = mx;
    int ly = my;

    // sadece LMB press ile tık
    if (!(pr & 1)) return;

    bool file_btn_hit = (lx >= 0 && lx <= 60 && ly >= 0 && ly <= 20);
    if (file_btn_hit) {
        data->menu_open = !data->menu_open;

        data->menu_hover_item = -1;
        data->last_lx = -999;
        data->last_ly = -999;

        // ✅ aç/kapat anında çiz
        desktop_damage_rect(client.x + 5, client.y + 2, 55, 16);     // Dosya butonu
        desktop_damage_rect(client.x + 5, client.y + 20, 110, 72);   // dropdown
        return;
    }

    if (data->menu_open) {
        int m_x = 5, m_y = 20;
        int m_w = 110, m_h = 72;

        bool menu_area_hit = (lx >= m_x && lx <= m_x + m_w &&
                              ly >= m_y && ly <= m_y + m_h);

        if (menu_area_hit) {
            int item = (ly - m_y - 5) / 16;
            data->menu_open = false;
            desktop_damage_rect(client.x + 5, client.y + 20, 110, 72);
            desktop_damage_rect(client.x + 5, client.y + 2, 55, 16);

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

    (void)btn;
}

// ------------------------------------------------------------
// KEY (smart edit + selection)
// ------------------------------------------------------------
static void notepad_on_key(app_t* self, uint16_t scancode) {
    if (!self || !self->user) return;

    notepad_t* data = (notepad_t*)self->user;
    notepad_tab_t* tab = ntab(data);
    if (!tab) return;

    if (save_dialog_is_active()) return;
    if (open_dialog_is_active()) return;
    if (messagebox_is_visible()) return;

    uint8_t is_e0 = ((scancode & 0xFF00) == 0xE000);
    uint8_t sc    = (uint8_t)(scancode & 0xFF);

    // break gelirse ignore
    if (sc & 0x80) return;

    int shift = kbd_is_shift_pressed();
    
    #define NP_DAMAGE_CLIENT() do { \
        ui_rect_t rc = wm_get_client_rect(self->win_id); \
        desktop_damage_rect(rc.x, rc.y, rc.w, rc.h); \
        printk("[NP] damage rc=%d,%d %dx%d\n", rc.x, rc.y, rc.w, rc.h); \
    } while (0)

    // ✅ Arrow keys (E0)
    if (is_e0) {
        bool moved = false;

        if (sc == 0x4B) { // Left
            if (!shift) {
                if (np_sel_has(tab)) { np_collapse_selection(tab, -1); moved = true; }
                else {
                    np_sel_clear(tab);
                    np_move_cursor_lr(tab, -1);
                    np_sel_clear(tab);
                    moved = true;
                }
            } else {
                if (!tab->sel_active) { tab->sel_active = 1; tab->sel_anchor = tab->cursor; }
                np_move_cursor_lr(tab, -1);
                tab->sel_end = tab->cursor;
                moved = true;
            }

            if (moved) NP_DAMAGE_CLIENT();
            return;
        }

        if (sc == 0x4D) { // Right
            if (!shift) {
                if (np_sel_has(tab)) { np_collapse_selection(tab, +1); moved = true; }
                else {
                    np_sel_clear(tab);
                    np_move_cursor_lr(tab, +1);
                    np_sel_clear(tab);
                    moved = true;
                }
            } else {
                if (!tab->sel_active) { tab->sel_active = 1; tab->sel_anchor = tab->cursor; }
                np_move_cursor_lr(tab, +1);
                tab->sel_end = tab->cursor;
                moved = true;
            }

            if (moved) NP_DAMAGE_CLIENT();
            return;
        }

        if (sc == 0x53) { // Delete
            bool changed = np_delete_forward(tab);
            if (changed) {
                tab->is_dirty = true;
                np_sel_clear(tab);
                NP_DAMAGE_CLIENT();
            }
            return;
        }

        if (sc == 0x48) { // Up
            if (!shift) {
                if (np_sel_has(tab)) np_sel_clear(tab);
                np_move_cursor_up(tab);
                np_sel_clear(tab);
            } else {
                if (!tab->sel_active) { tab->sel_active = 1; tab->sel_anchor = tab->cursor; }
                np_move_cursor_up(tab);
                tab->sel_end = tab->cursor;
            }
            NP_DAMAGE_CLIENT();
            return;
        }

        if (sc == 0x50) { // Down
            if (!shift) {
                if (np_sel_has(tab)) np_sel_clear(tab);
                np_move_cursor_down(tab);
                np_sel_clear(tab);
            } else {
                if (!tab->sel_active) { tab->sel_active = 1; tab->sel_anchor = tab->cursor; }
                np_move_cursor_down(tab);
                tab->sel_end = tab->cursor;
            }
            NP_DAMAGE_CLIENT();
            return;
        }
    }

    bool changed = false;

    // TAB / ENTER / BACKSPACE
    if (sc == 0x0F) {
        changed = np_insert_char(tab, '\t');
    }
    else if (sc == 0x1C) {
        changed = np_smart_enter(tab);
    }
    else if (sc == 0x0E) {
        changed = np_backspace(tab);
    }
    else {
        char c = kbd_scancode_to_ascii(sc);

        if (c) {
            if (c == ' ') {
                if (np_sel_has(tab)) np_delete_selection(tab);
                changed = np_insert_char(tab, ' ');
                if (changed) np_try_fold_4spaces_to_tab(tab);
            }
            else if (c == '{') {
                changed = np_open_brace_pair(tab);
            }
            else if (c == '[') {
                changed = np_open_bracket_pair(tab);
            }
            else if (c >= 32 && c <= 126) {
                if (np_sel_has(tab)) np_delete_selection(tab);
                changed = np_insert_char(tab, c);
            }
        }
    }

    if (changed) {
        tab->is_dirty = true;
        np_sel_clear(tab);
        NP_DAMAGE_CLIENT();   // ✅ EN ÖNEMLİ SATIR: yazı görünmesi için
    }

    #undef NP_DAMAGE_CLIENT

    printk("[NP] key changed=%d len=%u\n", changed, (unsigned)strlen(tab->text));
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