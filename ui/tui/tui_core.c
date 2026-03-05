#include <ui/tui/tui.h>
#include <ui/tui/tui_action.h>
#include <ui/tui/tui_cfg.h>
#include <ui/tui/tui_input.h>

#include <ui/notification.h>

#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/video/fb.h>

#include <kernel/fs/vfs.h>

#include <kernel/memory/kmalloc.h>

#include <lib/string.h>
#include <stdint.h>

#define TUI_MAX_ITEMS   16
#define TUI_TITLE_MAX   64
#define TUI_LABEL_MAX   64
#define TUI_ACTION_MAX  64

typedef struct {
    char label[TUI_LABEL_MAX];
    char action[TUI_ACTION_MAX];
} tui_item_t;

static tui_item_t g_items[TUI_MAX_ITEMS];
static int g_item_count = 0;
static int g_selected = 0;

static char g_title[TUI_TITLE_MAX] = "Menu";

static int g_e0 = 0;

static void tui_draw_notification(void);

void tui_clear(void) {
    g_item_count = 0;
    g_selected = 0;
    g_title[0] = 0;
}

void tui_set_title(const char* t) {
    if (!t) t = "Menu";
    strncpy(g_title, t, sizeof(g_title) - 1);
    g_title[sizeof(g_title) - 1] = 0;
}

void tui_add_item(const char* label, const char* action) {
    if (!label) label = "";
    if (!action) action = "";

    if (g_item_count >= TUI_MAX_ITEMS) return;

    tui_item_t* it = &g_items[g_item_count];

    strncpy(it->label, label, sizeof(it->label) - 1);
    it->label[sizeof(it->label) - 1] = 0;

    strncpy(it->action, action, sizeof(it->action) - 1);
    it->action[sizeof(it->action) - 1] = 0;

    g_item_count++;
}

static void tui_draw(void) {
    gfx_clear(0x00202020);

    int box_w = 360;
    int box_h = 220;
    int x = ((int)fb_get_width()  - box_w) / 2;
    int y = ((int)fb_get_height() - box_h) / 2;

    gfx_fill_rect(x, y, box_w, box_h, 0x00262626);
    gfx_draw_rect(x, y, box_w, box_h, 0x00404040);

    gfx_draw_text_utf8(x + 20, y + 18, 0x00FFFFFF, g_title);

    int iy = y + 60;

    for (int i = 0; i < g_item_count; i++) {
        uint32_t txt = 0x00CCCCCC;

        if (i == g_selected) {
            gfx_fill_rect(x + 14, iy - 4, box_w - 28, 20, 0x00404040);
            txt = 0x00FFFFFF;
        }

        gfx_draw_text_utf8(x + 24, iy, txt, g_items[i].label);
        iy += 22;
    }

    tui_draw_notification();

    // notification_draw();

    fb_present();
}

void tui_init(void) {
    tui_draw();
}

void tui_tick(void) {
    if (tui_input_is_active()) {
        tui_input_tick();
        return;
    }

    notification_tick(16);
    if (notification_is_visible()) tui_draw();
}

void tui_handle_scancode(uint16_t sc)
{
    if (tui_input_is_active()) {
        tui_input_handle_scancode(sc);
        return;
    }

    if (sc == 0xE048) { // UP
        if (g_selected > 0)
            g_selected--;
        tui_draw();
        return;
    }

    if (sc == 0xE050) { // DOWN
        if (g_selected < g_item_count - 1)
            g_selected++;
        tui_draw();
        return;
    }

    if ((sc & 0xFF) == 0x1C) { // ENTER
        tui_execute_action(g_items[g_selected].action);
    }
}

int tui_get_item_count(void) { return g_item_count; }
int tui_get_selected(void) { return g_selected; }

void tui_set_selected(int idx) {
    if (idx < 0) idx = 0;
    if (idx >= g_item_count) idx = g_item_count ? (g_item_count - 1) : 0;
    g_selected = idx;
}

static char g_note[96];
static int  g_note_ms = 0;     // kalan süre
static int  g_note_dirty = 0;  // redraw isteği

void tui_notify(const char* msg, int ms) {
    if (!msg) msg = "";
    strncpy(g_note, msg, sizeof(g_note)-1);
    g_note[sizeof(g_note)-1] = 0;
    g_note_ms = (ms <= 0) ? 1500 : ms;
    g_note_dirty = 1;
}

static void tui_draw_notification(void) {
    if (g_note_ms <= 0 || !g_note[0]) return;

    int pad = 10;
    int w = 420;
    int h = 28;

    int x = ((int)fb_get_width() - w) / 2;
    int y = (int)fb_get_height() - h - 30;

    // arka kutu
    gfx_fill_rect(x, y, w, h, 0x00303030);
    gfx_draw_rect(x, y, w, h, 0x004A4A4A);

    // yazı
    gfx_draw_text_utf8(x + pad, y + 6, 0x00FFFFFF, g_note);
}