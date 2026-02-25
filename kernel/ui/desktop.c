// kernel/ui/desktop.c
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

    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }

    int W = (int)fb_get_width();
    int H = (int)fb_get_height();

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

// ✅ Klavye/hotkey ile UI değişince bir kere full present zorla
static bool g_force_full_present = false;

static void desktop_toggle_ext(void);

void desktop_invalidate_full(void) {
    g_force_full_present = true;
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
    desktop_icons_snap_all();      // ✅ iyi olur, dizilim bozulmasın
    desktop_invalidate_full();     // ✅ senin helper, g_force_full_present = true
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
    desktop_seed_default_shortcuts(false);

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
    char c = kbd_scancode_to_ascii(sc8);

    // ignore break (key up)
    if (sc8 & 0x80) {
        return;
    }

    // ------------------------------------------------------------
    // GLOBAL HOTKEY: SUPER+R -> Run (app id=7)
    // Set1: R make = 0x13
    // ------------------------------------------------------------
    if (kbd_is_super_pressed() && sc8 == 0x13) {
        app_t* a = appmgr_start_app(7);
        if (a) wm_set_active_id(a->win_id); // sende bu var
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
    int dx, dy;
    int wheel = 0;
    uint8_t btn;

    // --- Dirty-rect tracking (cursor + selection) ---
    static int prev_mouse_x = -1;
    static int prev_mouse_y = -1;
    static bool prev_selecting = false;
    static int prev_sel_start_x = 0, prev_sel_start_y = 0;
    static int prev_sel_end_x = 0, prev_sel_end_y = 0;

    bool need_full_present = false;

    // Bu tick'te mouse event geldi mi?
    bool had_mouse_event = false;

    // ---------- Mouse ----------
    ps2_mouse_poll();

    // Eğer hiç event yoksa bile btn state'ini bilmek bazen lazım olabilir.
    btn = g_last_btn;

    while (ps2_mouse_pop(&dx, &dy, &wheel, &btn)) {
        had_mouse_event = true;

        mouse_x += dx;
        mouse_y += dy;
        g_dbg_last_dx = dx;
        g_dbg_last_dy = dy;

        if (mouse_x < 0) mouse_x = 0;
        if (mouse_y < 0) mouse_y = 0;
        if (mouse_x > (int)(fb_get_width() - 1))  mouse_x = (int)fb_get_width() - 1;
        if (mouse_y > (int)(fb_get_height() - 1)) mouse_y = (int)fb_get_height() - 1;

        // ---- WHEEL ----
        if (wheel != 0) {
            int step = (wheel > 0) ? -1 : 1;
            g_dbg_wheel_step = step;
            g_dbg_wheel_total += step;
            if (g_dbg_wheel_total > 500) g_dbg_wheel_total = 500;
            if (g_dbg_wheel_total < -500) g_dbg_wheel_total = -500;

            wm_handle_mouse_wheel(mouse_x, mouse_y, step, btn);
            need_full_present = true;
        }

        if (dx != 0 || dy != 0) {
            wm_handle_mouse_move(mouse_x, mouse_y);

            // Mouse bir pencerenin üstündeyse hover değişebilir -> full present gerekli
            if (wm_find_window_at(mouse_x, mouse_y) != -1) {
                need_full_present = true;
            }
        }

        uint8_t pressed  = btn & ~g_last_btn;
        uint8_t released = g_last_btn & ~btn;

        // 0) Modal dialoglar
        if (save_dialog_is_active()) {
            need_full_present = true;
            save_dialog_handle_mouse(mouse_x, mouse_y, (pressed & 1));
            g_last_btn = btn;
            continue;
        }
        if (open_dialog_is_active()) {
            need_full_present = true;
            open_dialog_handle_mouse(mouse_x, mouse_y, (pressed & 1));
            g_last_btn = btn;
            continue;
        }

        // 1) Topbar
        if ((pressed & 1) && mouse_y < 28) {
            need_full_present = true;
            topbar_handle_mouse(mouse_x, mouse_y);
            g_last_btn = btn;
            continue;
        }

        // 2) Messagebox
        messagebox_handle_mouse(mouse_x, mouse_y, (pressed & 1));
        if (messagebox_is_visible()) {
            need_full_present = true;
            g_last_btn = btn;
            continue;
        }

        // 3) WM
        wm_handle_mouse(mouse_x, mouse_y, pressed, released, btn);
        if (wm_did_consume_mouse()) {
            need_full_present = true;
            is_selecting = false;
            g_lmb_down = 0;
            g_dragging = 0;
            g_down_hit = -1;
            g_last_btn = btn;
            continue;
        }

        // 4) Desktop alanı
        if (!wm_is_any_window_captured()) {

            // ✅ Context menu açıkken: her eventte hover/submenu update
            if (context_menu_is_visible()) {
                need_full_present = true; // dirty-rect menü için güvenli değil
                context_menu_handle_mouse(mouse_x, mouse_y, false);
            }

            // Sağ tık: context menu (yeniden kur + aç)
            if (pressed & 2) {
                need_full_present = true;

                // Desktop input state reset
                g_lmb_down = 0;
                g_dragging = 0;
                g_down_hit = -1;
                is_selecting = false;

                int hit = desktop_icons_get_hit(mouse_x, mouse_y);

                // ✅ Windows gibi: sağ tık ikon üstündeyse onu seç
                desktop_icons_deselect_all();
                if (hit != -1) desktop_icons_select(hit);

                context_menu_reset();

                if (hit != -1) {
                    // ikon üstü
                    context_menu_add_item("Ac", desktop_handle_open);
                    context_menu_add_item("Ad Degistir", desktop_handle_rename);
                    context_menu_add_item("Sil", desktop_icons_delete_selected);

                    // (istersen ikon üstünde de Görünüm koy)
                    // context_menu_t* view = context_menu_add_submenu("Gorunum");
                    // context_menu_add_item_to(view, "Dosya uzantilarini goster", desktop_toggle_ext);
                } else {
                    // boş alan
                    context_menu_t* view = context_menu_add_submenu("Gorunum");
                    context_menu_add_item_to(view,
                        ui_get_show_extensions() ? "Dosya uzantilarini gizle" : "Dosya uzantilarini goster",
                        desktop_toggle_ext);

                    context_menu_add_item("Yeni Metin Belgesi", desktop_handle_create_file);
                    context_menu_add_item("Yeni Klasor", desktop_handle_create_folder);
                }

                context_menu_show(mouse_x, mouse_y);

                g_last_btn = btn;
                continue;
            }

            // ✅ Menü açıkken: sol tık pressed menüye gider, desktop’a geçmez
            if (context_menu_is_visible()) {
                if (pressed & 1) {
                    need_full_present = true;
                    context_menu_handle_mouse(mouse_x, mouse_y, true);
                    g_last_btn = btn;
                    continue;
                }

                // Menü açıkken desktop drag/selection yok
                g_last_btn = btn;
                continue;
            }

            // ------------------------------------------------------------
            // Menü kapalıysa normal desktop input
            // ------------------------------------------------------------

            // Sol tık pressed
            if (pressed & 1) {
                need_full_present = true;

                int hit = desktop_icons_get_hit(mouse_x, mouse_y);

                g_lmb_down = 1;
                g_dragging = 0;
                g_down_x = mouse_x;
                g_down_y = mouse_y;
                g_down_hit = hit;

                desktop_icons_deselect_all();

                if (hit != -1) {
                    desktop_icons_select(hit);

                    uint32_t now = g_ticks_ms;
                    if (g_last_click_hit == hit && (now - g_last_click_ms) < DBLCLICK_MS) {
                        desktop_icons_process_click(hit);

                        g_last_click_hit = -1;
                        g_last_click_ms = 0;
                        g_lmb_down = 0;
                        g_dragging = 0;
                        g_down_hit = -1;

                        g_last_btn = btn;
                        continue;
                    } else {
                        g_last_click_hit = hit;
                        g_last_click_ms = now;
                    }
                } else {
                    is_selecting = true;
                    sel_start_x = mouse_x;
                    sel_start_y = mouse_y;

                    g_last_click_hit = -1;
                    g_last_click_ms = 0;
                }
            }

            // Sol tık basılı: drag threshold
            if (btn & 1) {
                if (g_lmb_down && g_down_hit != -1 && !g_dragging) {
                    int ddx = mouse_x - g_down_x;
                    int ddy = mouse_y - g_down_y;
                    if ((ddx * ddx + ddy * ddy) >= (DRAG_THRESHOLD_PX * DRAG_THRESHOLD_PX)) {
                        need_full_present = true;
                        g_dragging = 1;
                        desktop_icons_set_dragging(g_down_hit, true);
                        is_selecting = false;
                    }
                }
                if (g_dragging) {
                    need_full_present = true;
                    desktop_icons_move_dragging(mouse_x, mouse_y);
                }
            }

            // Sol tık release
            if (released & 1) {
                need_full_present = true;
                g_lmb_down = 0;

                if (g_dragging) {
                    g_dragging = 0;
                    g_down_hit = -1;
                    desktop_icons_stop_dragging_all();
                    desktop_icons_snap_all();
                    is_selecting = false;

                    g_last_btn = btn;
                    continue;
                }

                if (is_selecting) {
                    desktop_icons_select_in_rect(sel_start_x, sel_start_y, mouse_x, mouse_y);
                }
                is_selecting = false;

                desktop_icons_stop_dragging_all();
                desktop_icons_snap_all();
                g_down_hit = -1;
            }
        }

        g_last_btn = btn;
    }

    // ---------- Render ----------
    // ✅ Klavye/hotkey ile UI değiştiyse bu frame full present zorla
    if (g_force_full_present) {
        need_full_present = true;
        g_force_full_present = false;
    }

    if (g_dbg_overlay) need_full_present = true;
    if (memmon_is_visible()) need_full_present = true;

    if (wm_is_dragging_window()) need_full_present = true;

    fb_clear(ui_get_desktop_bg());
    desktop_icons_draw_all();
    topbar_draw();

    if (is_selecting) {
        gfx_draw_alpha_rect(
            abs(mouse_x - sel_start_x),
            abs(mouse_y - sel_start_y),
            0, 85, 170, 150,
            min(sel_start_x, mouse_x),
            min(sel_start_y, mouse_y)
        );
    }

    static int once = 0;
    if (!once) { once = 1; printk("[NOTIF] draw called\n"); }

    wm_draw();
    save_dialog_draw();
    open_dialog_draw();
    context_menu_draw();
    messagebox_draw();
    notification_draw();

    memmon_draw((int)fb_get_width(), (int)fb_get_height());   // ✅ BURAYA

    cursor_draw_arrow(mouse_x, mouse_y);
    dbg_draw_panel();

    // ---------- Present ----------
    const int CW = 32;
    const int CH = 32;
    const int CPAD = 10;

    if (need_full_present || prev_mouse_x < 0) {
        fb_present();
    } else {
        // Cursor old+new
        present_rect_safe(prev_mouse_x - CPAD, prev_mouse_y - CPAD, CW + CPAD * 2, CH + CPAD * 2);
        present_rect_safe(mouse_x      - CPAD, mouse_y      - CPAD, CW + CPAD * 2, CH + CPAD * 2);

        // Selection old+new
        if (prev_selecting || is_selecting) {
            int ax0 = prev_sel_start_x, ay0 = prev_sel_start_y;
            int ax1 = prev_sel_end_x,   ay1 = prev_sel_end_y;

            int bx0 = sel_start_x,      by0 = sel_start_y;
            int bx1 = mouse_x,          by1 = mouse_y;

            int a_x = (ax0 < ax1) ? ax0 : ax1;
            int a_y = (ay0 < ay1) ? ay0 : ay1;
            int a_w = (ax0 < ax1) ? (ax1 - ax0) : (ax0 - ax1);
            int a_h = (ay0 < ay1) ? (ay1 - ay0) : (ay0 - ay1);

            int b_x = (bx0 < bx1) ? bx0 : bx1;
            int b_y = (by0 < by1) ? by0 : by1;
            int b_w = (bx0 < bx1) ? (bx1 - bx0) : (bx0 - bx1);
            int b_h = (by0 < by1) ? (by1 - by0) : (by0 - by1);

            const int PAD = 6;
            present_rect_safe(a_x - PAD, a_y - PAD, a_w + PAD * 2, a_h + PAD * 2);
            present_rect_safe(b_x - PAD, b_y - PAD, b_w + PAD * 2, b_h + PAD * 2);
        }
    }

    // Save prev state
    prev_mouse_x = mouse_x;
    prev_mouse_y = mouse_y;
    prev_selecting = is_selecting;
    prev_sel_start_x = sel_start_x;
    prev_sel_start_y = sel_start_y;
    prev_sel_end_x = mouse_x;
    prev_sel_end_y = mouse_y;
}

// ============================================================
// Legacy blocking API (kalsın ama kullanma)
// ============================================================
void ui_desktop_run(void) {
    ui_desktop_init();
    while (1) {
        ui_desktop_tick();
        asm volatile("hlt");
    }
}