#include <app/app.h>
#include <ui/wm.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/memory/kmalloc.h>
#include <lib/string.h>
#include <stdint.h>

// --- 1. VERİ YAPILARI ---
#define MAX_FILES 32
#define SIDEBAR_COUNT 4

typedef struct {
    char name[64];
    int is_dir;
} fm_item_t;

typedef struct {
    char path[128];
    fm_item_t items[MAX_FILES];
    int count;
    int selected_idx;

    const char* sidebar_items[SIDEBAR_COUNT];
    int selected_sidebar;
    int focus_on_sidebar; 
} fm_state_t;

// --- 2. YARDIMCI METODLAR ---

// Eğer lib/string.h içinde strstr yoksa hata almamak için basit bir iç strstr implementation
static const char* _internal_strstr(const char* haystack, const char* needle) {
    if (!*needle) return haystack;
    for (; *haystack; haystack++) {
        if (*haystack == *needle) {
            const char *h = haystack, *n = needle;
            while (*h && *n && *h == *n) { h++; n++; }
            if (!*n) return haystack;
        }
    }
    return 0;
}

static void fm_add_item(fm_state_t* state, const char* name, int is_dir) {
    if (state->count >= MAX_FILES) return;
    strncpy(state->items[state->count].name, name, 63);
    state->items[state->count].is_dir = is_dir;
    state->count++;
}

static void fm_navigate_to(fm_state_t* state, const char* new_path) {
    strncpy(state->path, new_path, 127);
    state->count = 0;
    state->selected_idx = 0;

    // Geri gitme seçeneği
    if (strcmp(new_path, "/toyfs/home") != 0 && strcmp(new_path, "Ana Klasor") != 0) {
        fm_add_item(state, "..", 1);
    }

    // Klasör içeriği simülasyonu (_internal_strstr kullanarak)
    if (_internal_strstr(new_path, "Ana Klasor") || strcmp(new_path, "/toyfs/home") == 0) {
        fm_add_item(state, "Belgeler", 1);
        fm_add_item(state, "Resimler", 1);
        fm_add_item(state, "readme.txt", 0);
    } 
    else if (_internal_strstr(new_path, "Belgeler")) {
        fm_add_item(state, "notlar.txt", 0);
        fm_add_item(state, "proje.c", 0);
    }
    else if (_internal_strstr(new_path, "Resimler")) {
        fm_add_item(state, "manzara.bmp", 0);
        fm_add_item(state, "logo.png", 0);
    }
    else {
        fm_add_item(state, "(Bos)", 0);
    }
}

// --- 3. APP OLAYLARI ---

void fm_on_create(app_t* app) {
    if (!app) return;
    fm_state_t* state = (fm_state_t*)kmalloc(sizeof(fm_state_t));
    if (!state) return;

    state->sidebar_items[0] = "Ana Klasor";
    state->sidebar_items[1] = "Sistem (toyfs)";
    state->sidebar_items[2] = "Depo (persist)";
    state->sidebar_items[3] = "Gecici (ram)";
    
    state->selected_sidebar = 0;
    state->focus_on_sidebar = 1;

    fm_navigate_to(state, "/toyfs/home");
    app->user_data = state;

    extern ui_window_t* wm_get_window_ptr(int win_id);
    ui_window_t* win = wm_get_window_ptr(app->win_id);
    if (win) { win->has_chrome = 1; win->has_close = 1; }
}

void fm_on_draw(app_t* app) {
    if (!app || !app->user_data) return;
    fm_state_t* state = (fm_state_t*)app->user_data;
    ui_rect_t r = wm_get_client_rect(app->win_id);
    int sidebar_w = 140;

    gfx_fill_rect(r.x, r.y, r.w, r.h, 0xFFFFFFFF);
    gfx_fill_rect(r.x, r.y, sidebar_w, r.h, 0xFFF2F2F2);
    gfx_draw_line(r.x + sidebar_w, r.y, r.x + sidebar_w, r.y + r.h, 0xFFCCCCCC);

    // Sidebar
    for (int i = 0; i < SIDEBAR_COUNT; i++) {
        int sy = r.y + 40 + (i * 25);
        if (state->focus_on_sidebar && i == state->selected_sidebar)
            gfx_fill_rect(r.x + 5, sy - 4, sidebar_w - 10, 20, 0xFFD0E0FF);
        gfx_draw_text(r.x + 15, sy, 0xFF000000, state->sidebar_items[i]);
    }

    // Liste
    int list_x = r.x + sidebar_w + 10;
    gfx_fill_rect(list_x - 10, r.y, r.w - sidebar_w, 25, 0xFFE0E0E0);
    gfx_draw_text(list_x, r.y + 6, 0xFF000088, state->path);

    for (int i = 0; i < state->count; i++) {
        int ty = r.y + 40 + (i * 22);
        if (!state->focus_on_sidebar && i == state->selected_idx)
            gfx_fill_rect(list_x - 5, ty - 2, r.w - sidebar_w - 10, 20, 0xFFE8F0FF);

        const char* type = state->items[i].is_dir ? " ├ [D] " : " ├ [F] ";
        gfx_draw_text(list_x, ty, state->items[i].is_dir ? 0xFF0000AA : 0xFF444444, type);
        gfx_draw_text(list_x + 55, ty, 0xFF000000, state->items[i].name);
    }
}

void fm_on_key(app_t* app, uint16_t key) {
    fm_state_t* state = (fm_state_t*)app->user_data;
    if (!state) return;

    if (key == '\t') state->focus_on_sidebar = !state->focus_on_sidebar;

    if (state->focus_on_sidebar) {
        if (key == 0xE048 && state->selected_sidebar > 0) state->selected_sidebar--;
        if (key == 0xE050 && state->selected_sidebar < SIDEBAR_COUNT-1) state->selected_sidebar++;
    } else {
        if (key == 0xE048 && state->selected_idx > 0) state->selected_idx--;
        if (key == 0xE050 && state->selected_idx < state->count-1) state->selected_idx++;
    }

    if (key == '\n' || key == '\r') {
        if (state->focus_on_sidebar) {
            fm_navigate_to(state, state->sidebar_items[state->selected_sidebar]);
            state->focus_on_sidebar = 0;
        } else {
            fm_item_t* sel = &state->items[state->selected_idx];
            if (sel->is_dir) {
                if (strcmp(sel->name, "..") == 0) fm_navigate_to(state, "/toyfs/home");
                else {
                    char next[128];
                    strcpy(next, state->path);
                    if (next[strlen(next)-1] != '/') strcat(next, "/");
                    strcat(next, sel->name);
                    fm_navigate_to(state, next);
                }
            }
        }
    }
}

const app_vtbl_t file_manager_app = {
    .on_create = fm_on_create,
    .on_draw = fm_on_draw,
    .on_key = fm_on_key,
};