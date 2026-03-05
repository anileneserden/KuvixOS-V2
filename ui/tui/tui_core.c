// ui/tui.c
#include <ui/tui/tui.h>
#include <ui/tui/tui_action.h>

#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/printk.h>

#include <stdint.h>

#define TUI_MAX_ITEMS 16

typedef struct {
    const char* label;   // ekranda görünen
    const char* action;  // "session:tty1", "sys:reboot" vs
} tui_item_t;

static tui_item_t g_items[TUI_MAX_ITEMS];
static int g_item_count = 0;
static int g_selected   = 0;

static const char* g_title = "Menu";

static int g_e0 = 0;

/* Enter ile seçilen action buraya düşer */
static const char* g_pending_action = 0;

void tui_set_title(const char* t) {
    if (t && t[0]) g_title = t;
}

void tui_add_item(const char* label, const char* action) {
    if (!label || !label[0]) return;
    if (g_item_count >= TUI_MAX_ITEMS) return;

    g_items[g_item_count].label  = label;
    g_items[g_item_count].action = action ? action : "";
    g_item_count++;
}

static void tui_draw(void) {
    gfx_clear(0x00202020);

    int x = 40;
    int y = 40;

    // title
    gfx_draw_text_utf8(x, y, 0x00FFFFFF, g_title);
    y += 40;

    // items
    for (int i = 0; i < g_item_count; i++) {
        uint32_t color = 0x00CCCCCC;

        if (i == g_selected) {
            color = 0x00FFFFFF;
            // selection bg
            gfx_fill_rect(x - 10, y - 4, 260, 20, 0x00404040);
        }

        gfx_draw_text_utf8(x, y, color, g_items[i].label);
        y += 20;
    }

    fb_present();
}

void tui_init(void) {
    g_item_count = 0;
    g_selected = 0;
    g_e0 = 0;
    g_pending_action = 0;

    // demo default menu (istersen bunu dışarıdan kur)
    tui_set_title("KuvixOS");
    tui_add_item("Terminal",  "session:tty1");
    tui_add_item("Desktop",   "session:desktop");
    tui_add_item("Reboot",    "sys:reboot");
    tui_add_item("Power Off", "sys:poweroff");

    tui_draw();
}

void tui_tick(void) {
    // şimdilik yok (animasyon/cursor blink vs eklenebilir)
}

/* Enter basılınca action’ı buradan alacaksın */
const char* tui_take_action(void) {
    const char* a = g_pending_action;
    g_pending_action = 0;
    return a;
}

static void tui_move_up(void) {
    if (g_item_count <= 0) return;
    if (g_selected > 0) g_selected--;
    tui_draw();
}

static void tui_move_down(void) {
    if (g_item_count <= 0) return;
    if (g_selected < g_item_count - 1) g_selected++;
    tui_draw();
}

static void tui_select(void) {
    if (g_item_count <= 0) return;
    g_pending_action = g_items[g_selected].action;
    // İstersen seçince küçük bir görsel feedback de ekleyebiliriz
}

void tui_handle_scancode(uint16_t sc)
{
    uint8_t code = (uint8_t)(sc & 0xFF);

    // debug
    printk("tui sc=%x code=%x\n", sc, code);

    // Eğer event formatın "E0"yu üst byte'a koyuyorsa (senin eski kodda vardı)
    // E0 = 0xE000 gibi geliyorsa:
    if ((sc & 0xFF00) == 0xE000) {
        // bu durumda code zaten 0x48/0x50 vs olur
        if (code == 0x48) { // Up
            if (g_selected > 0) g_selected--;
            tui_draw();
            return;
        }
        if (code == 0x50) { // Down
            if (g_selected < g_item_count - 1) g_selected++;
            tui_draw();
            return;
        }
        return;
    }

    // Eğer driver E0'yu ayrı byte olarak yolluyorsa:
    if (code == 0xE0) { g_e0 = 1; return; }

    // break ignore
    if (code & 0x80) { g_e0 = 0; return; }

    // E0 arrow (separate prefix mode)
    if (g_e0) {
        if (code == 0x48) { if (g_selected > 0) g_selected--; tui_draw(); }
        else if (code == 0x50) { if (g_selected < g_item_count - 1) g_selected++; tui_draw(); }
        g_e0 = 0;
        return;
    }

    // W/S (normal, E0'suz)
    if (code == 0x11) { // W
        if (g_selected > 0) g_selected--;
        tui_draw();
        return;
    }
    if (code == 0x1F) { // S
        if (g_selected < g_item_count - 1) g_selected++;
        tui_draw();
        return;
    }

    // Enter
    if (code == 0x1C) {
        tui_execute_action(g_items[g_selected].action);
        return;
    }
}