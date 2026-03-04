#include <ui/controls/combobox2.h>
#include <kernel/drivers/video/gfx.h>
#include <lib/string.h>

static int clampi(int v, int a, int b) { if (v < a) return a; if (v > b) return b; return v; }

static bool pt_in(int mx, int my, int x, int y, int w, int h) {
    return (mx >= x && mx < x + w && my >= y && my < y + h);
}

static void draw_arrow(int x, int y, uint32_t col) {
    // basit v şekli
    gfx_fill_rect(x + 0, y + 0, 1, 1, col);
    gfx_fill_rect(x + 1, y + 1, 1, 1, col);
    gfx_fill_rect(x + 2, y + 2, 1, 1, col);
    gfx_fill_rect(x + 3, y + 1, 1, 1, col);
    gfx_fill_rect(x + 4, y + 0, 1, 1, col);
}

void ui_combobox2_init(ui_combobox2_t* cb, int x, int y, int w, int h) {
    if (!cb) return;
    memset(cb, 0, sizeof(*cb));
    cb->x = x; cb->y = y; cb->w = w; cb->h = h;

    cb->item_count = 0;
    cb->selected = -1;
    cb->open = false;
    cb->hover_index = -1;

    cb->bg = 0xFFF8F8F8;
    cb->border = 0xFFB0B0B0;
    cb->text_col = 0xFF111111;
    cb->sel_bg = 0xFF0055AA;
    cb->sel_text = 0xFFFFFFFF;

    cb->item_h = 22;
    cb->max_visible = 8;
}

void ui_combobox2_add_item(ui_combobox2_t* cb, const char* text) {
    if (!cb || !text) return;
    if (cb->item_count >= (int)(sizeof(cb->items) / sizeof(cb->items[0]))) return;
    cb->items[cb->item_count++] = text;

    // ilk item otomatik seçilsin istersen:
    if (cb->selected < 0) cb->selected = 0;
}

void ui_combobox2_set_selected(ui_combobox2_t* cb, int index) {
    if (!cb) return;
    if (cb->item_count <= 0) { cb->selected = -1; return; }
    index = clampi(index, 0, cb->item_count - 1);
    if (cb->selected == index) return;

    cb->selected = index;

    if (cb->on_change) {
        cb->on_change(cb->on_change_user, cb->selected, cb->items[cb->selected]);
    }
}

int ui_combobox2_get_selected(const ui_combobox2_t* cb) {
    return cb ? cb->selected : -1;
}

const char* ui_combobox2_get_selected_text(const ui_combobox2_t* cb) {
    if (!cb) return "";
    if (cb->selected < 0 || cb->selected >= cb->item_count) return "";
    return cb->items[cb->selected] ? cb->items[cb->selected] : "";
}

void ui_combobox2_set_on_change(ui_combobox2_t* cb, ui_combobox2_on_change_t fn, void* user) {
    if (!cb) return;
    cb->on_change = fn;
    cb->on_change_user = user;
}

static int dropdown_h(const ui_combobox2_t* cb) {
    if (!cb) return 0;
    int vis = cb->item_count;
    if (vis > cb->max_visible) vis = cb->max_visible;
    return vis * cb->item_h;
}

void ui_combobox2_draw(ui_combobox2_t* cb) {
    if (!cb) return;

    // main box
    gfx_fill_rect(cb->x, cb->y, cb->w, cb->h, cb->bg);
    gfx_draw_rect(cb->x, cb->y, cb->w, cb->h, cb->border);

    // selected text
    const char* t = ui_combobox2_get_selected_text(cb);
    int tx = cb->x + 8;
    int ty = cb->y + (cb->h / 2) - 7; // 16px font varsayımı gibi
    gfx_draw_text_utf8(tx, ty, cb->text_col, t);

    // arrow area
    int ax = cb->x + cb->w - 18;
    int ay = cb->y + (cb->h / 2) - 2;
    draw_arrow(ax, ay, cb->border);

    if (!cb->open) return;

    // dropdown box (hemen altına)
    int dh = dropdown_h(cb);
    int dx = cb->x;
    int dy = cb->y + cb->h;
    int dw = cb->w;

    gfx_fill_rect(dx, dy, dw, dh, 0xFFFFFFFF);
    gfx_draw_rect(dx, dy, dw, dh, cb->border);

    int vis = cb->item_count;
    if (vis > cb->max_visible) vis = cb->max_visible;

    for (int i = 0; i < vis; i++) {
        int iy = dy + i * cb->item_h;
        bool sel = (i == cb->selected);

        if (sel) {
            gfx_fill_rect(dx + 1, iy, dw - 2, cb->item_h, cb->sel_bg);
            gfx_draw_text_utf8(dx + 8, iy + 5, cb->sel_text, cb->items[i]);
        } else {
            gfx_fill_rect(dx + 1, iy, dw - 2, cb->item_h, 0xFFFFFFFF);
            gfx_draw_text_utf8(dx + 8, iy + 5, cb->text_col, cb->items[i]);
        }

        // row separator
        gfx_fill_rect(dx + 1, iy + cb->item_h - 1, dw - 2, 1, 0xFFEAEAEA);
    }
}

bool ui_combobox2_handle_mouse(ui_combobox2_t* cb, int mx, int my, uint8_t pressed, uint8_t released, uint8_t buttons) {
    (void)released; (void)buttons;
    if (!cb) return false;

    // sadece left click ile
    if (!(pressed & 1)) return false;

    // 1) main box click => toggle open
    if (pt_in(mx, my, cb->x, cb->y, cb->w, cb->h)) {
        cb->open = !cb->open;
        return true;
    }

    // 2) dropdown açıkken listeye tıklandı mı?
    if (cb->open) {
        int dx = cb->x;
        int dy = cb->y + cb->h;
        int dw = cb->w;
        int dh = dropdown_h(cb);

        if (pt_in(mx, my, dx, dy, dw, dh)) {
            int idx = (my - dy) / cb->item_h;
            int vis = cb->item_count;
            if (vis > cb->max_visible) vis = cb->max_visible;

            if (idx >= 0 && idx < vis) {
                ui_combobox2_set_selected(cb, idx);
            }
            cb->open = false;
            return true;
        }

        // dropdown dışına tık => kapat
        cb->open = false;
        return true;
    }

    return false;
}