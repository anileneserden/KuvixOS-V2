// kernel/ui/desktop.c

#include <ui/desktop.h>
#include <ui/desktop_icons.h>
#include <ui/messagebox.h>
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

static uint32_t desktop_bg_color = 0x182838;

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

static uint32_t g_last_click_ms = 0;
static int      g_last_click_hit = -1;

static const int      DRAG_THRESHOLD_PX = 6;
static const uint32_t DBLCLICK_MS = 350;

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

// ============================================================
// Desktop Handlers
// ============================================================

void desktop_handle_rename_confirm(const char* new_name) {
    if (rename_target_index == -1 || !new_name || strlen(new_name) == 0) return;

    const char* old_name = desktop_icons_get_name(rename_target_index);

    char old_full_path[256];
    char new_full_path[256];

    strcpy(old_full_path, USER_DESKTOP_PATH);
    strcat(old_full_path, "/");
    strcat(old_full_path, old_name);

    strcpy(new_full_path, USER_DESKTOP_PATH);
    strcat(new_full_path, "/");
    strcat(new_full_path, new_name);
    if (strstr(new_name, ".txt") == 0) strcat(new_full_path, ".txt");

    if (vfs_rename(old_full_path, new_full_path) == 1) {
        notification_show("Isim degistirildi", 500);
    } else {
        notification_show("Hata!", 1000);
    }

    desktop_icons_init();
    desktop_icons_snap_all();
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
    if (vfs_open(final_path, VFS_O_CREAT | VFS_O_RDWR, &f) == 1) {
        const char* hello = "Merhaba KuvixOS!\n";
        uint32_t w = 0;
        vfs_write(f, hello, (uint32_t)strlen(hello), &w);

        vfs_close(f);

        desktop_icons_init();
        desktop_icons_snap_all();

        int count = desktop_icons_get_count();
        if (count > 0) {
            rename_target_index = count - 1;
            desktop_icons_begin_edit(rename_target_index);
        }
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

    desktop_icons_init();
    desktop_icons_snap_all();

    appmgr_start_app(6);

    g_last_btn = 0;
    g_lmb_down = 0;
    g_dragging = 0;
    g_down_hit = -1;
    g_last_click_ms = 0;
    g_last_click_hit = -1;
    is_selecting = false;

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

void ui_desktop_handle_scancode(uint16_t sc) {
    char c = kbd_scancode_to_ascii((uint8_t)(sc & 0xFF));

    if (save_dialog_is_active()) { save_dialog_handle_key(sc, c); return; }
    if (open_dialog_is_active()) { open_dialog_handle_key(sc, c); return; }
    if (messagebox_is_visible()) return;

    if (desktop_icons_is_any_editing()) {
        desktop_icons_handle_key(sc, c);
        return;
    }

    int active_id = wm_get_active_id();
    app_t* active_app = appmgr_get_app_by_window_id(active_id);
    if (active_app && active_app->v && active_app->v->on_key) {
        active_app->v->on_key(active_app, sc);
    }
}

void ui_desktop_tick(void) {
    int dx, dy;
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
    // Ama ps2_mouse_pop() event üretmiyorsa btn güncellenmez; o yüzden btn'u varsayılanla başlat.
    btn = g_last_btn;

    while (ps2_mouse_pop(&dx, &dy, &btn)) {
        had_mouse_event = true;

        mouse_x += dx;
        mouse_y += dy;

        if (mouse_x < 0) mouse_x = 0;
        if (mouse_y < 0) mouse_y = 0;
        if (mouse_x > (int)(fb_get_width() - 1))  mouse_x = (int)fb_get_width() - 1;
        if (mouse_y > (int)(fb_get_height() - 1)) mouse_y = (int)fb_get_height() - 1;

        if (dx != 0 || dy != 0) {
            wm_handle_mouse_move(mouse_x, mouse_y);
        }

        if (dx != 0 || dy != 0) {
        // Mouse bir pencerenin üstündeyse hover değişebilir -> full present gerekli
        int over = wm_find_window_at(mouse_x, mouse_y);
        if (over != -1) need_full_present = true;
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

            // Sağ tık: context menu
            if (pressed & 2) {
                need_full_present = true;

                g_lmb_down = 0;
                g_dragging = 0;
                g_down_hit = -1;

                int hit = desktop_icons_get_hit(mouse_x, mouse_y);
                context_menu_reset();
                if (hit != -1) {
                    context_menu_add_item("Ac", desktop_handle_open);
                    context_menu_add_item("Ad Degistir", desktop_handle_rename);
                    context_menu_add_item("Sil", desktop_icons_delete_selected);
                } else {
                    context_menu_add_item("Yeni Metin Belgesi", desktop_handle_create_file);
                    context_menu_add_item("Yeni Klasör", desktop_handle_create_folder);
                }
                context_menu_show(mouse_x, mouse_y);
            }

            // Sol tık pressed
            if (pressed & 1) {
                need_full_present = true;

                if (context_menu_is_visible()) {
                    context_menu_handle_mouse(mouse_x, mouse_y, true);
                    g_last_btn = btn;
                    continue;
                }

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

    // Mouse event gelmediyse bile: hover state düzgün olsun diye WM'e move geçmek istersen:
    // (Bazı WM'ler hover için her tick ister. İstersen aç.)
    // if (!had_mouse_event) wm_handle_mouse_move(mouse_x, mouse_y);

    // ---------- Render ----------
    if (wm_is_dragging_window()) need_full_present = true;

    fb_clear(desktop_bg_color);
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

    wm_draw();
    save_dialog_draw();
    open_dialog_draw();
    context_menu_draw();
    messagebox_draw();
    notification_draw();
    cursor_draw_arrow(mouse_x, mouse_y);

    // ---------- Present ----------
    // Cursor rect'i “gerçek çizim” boyutundan biraz büyük bas: iz kalmasın.
    const int CW = 32;
    const int CH = 32;
    const int CPAD = 10; // 🔥 6 yerine 10 daha güvenli (antialias yok ama cursor şekli taşabilir)

    if (need_full_present || prev_mouse_x < 0) {
        fb_present();
    } else {
        // Cursor old+new (union mantığı)
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

            const int PAD = 6; // 4 -> 6 biraz daha güvenli
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
