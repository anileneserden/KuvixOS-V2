#include <ui/tui/tui.h>
#include <ui/tui/tui_action.h>
#include <ui/tui/tui_cfg.h>

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

    // Menü kutusu
    int box_w = 360;
    int box_h = 220;
    int x = ((int)fb_get_width()  - box_w) / 2;
    int y = ((int)fb_get_height() - box_h) / 2;

    gfx_fill_rect(x, y, box_w, box_h, 0x00262626);
    gfx_draw_rect(x, y, box_w, box_h, 0x00404040);

    // Title
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

    fb_present();
}

void tui_init(void) {
    tui_draw();
}

void tui_tick(void) {
}

void tui_handle_scancode(uint16_t sc)
{
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