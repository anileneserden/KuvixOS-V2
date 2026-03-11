// kernel/ui/apps/designer.c
#include <ui/apps/designer.h>

#include <app/app.h>               // ✅ struct app tamam (incomplete type hatası gider)
#include <ui/wm.h>
#include <kernel/drivers/video/gfx.h>
#include <lib/string.h>
#include <stdint.h>
#include <stdbool.h>

// ------------------------------
// Layout constants
// ------------------------------
#define TOOLBOX_W      170
#define PROPS_W        220
#define TOP_H          34
#define ITEM_H         28

// Preview window look
#define PREVIEW_MARGIN   16
#define PREVIEW_TITLE_H  26
#define PREVIEW_BORDER   2

// Default “form” size (center panel içinde clamp’lenir)
#define PREVIEW_DEF_W    520
#define PREVIEW_DEF_H    340

// Default object sizes
#define DEF_BTN_W      120
#define DEF_BTN_H      26
#define DEF_LBL_W      120
#define DEF_LBL_H      18

// ------------------------------
// helpers
// ------------------------------
static bool hit(int mx, int my, int x, int y, int w, int h) {
    return (mx >= x && mx < x + w && my >= y && my < y + h);
}

static void itoa_simple(int v, char* out, int cap) {
    if (!out || cap <= 0) return;
    out[0] = 0;

    char tmp[16];
    int n = 0;
    int neg = 0;

    if (v == 0) { out[0] = '0'; out[1] = 0; return; }
    if (v < 0) { neg = 1; v = -v; }

    while (v > 0 && n < (int)sizeof(tmp)) {
        tmp[n++] = (char)('0' + (v % 10));
        v /= 10;
    }

    int p = 0;
    if (neg && p < cap - 1) out[p++] = '-';
    while (n > 0 && p < cap - 1) out[p++] = tmp[--n];
    out[p] = 0;
}

static design_obj_t* find_obj_by_id(designer_t* d, int id) {
    if (!d || id <= 0) return 0;
    for (int i = 0; i < d->count; i++) {
        if (d->objs[i].id == id) return &d->objs[i];
    }
    return 0;
}

static void remove_obj_by_id(designer_t* d, int id) {
    if (!d || id <= 0) return;
    for (int i = 0; i < d->count; i++) {
        if (d->objs[i].id == id) {
            for (int j = i; j + 1 < d->count; j++) d->objs[j] = d->objs[j + 1];
            d->count--;
            if (d->selected_id == id) d->selected_id = -1;
            if (d->drag_id == id) { d->dragging = 0; d->drag_id = -1; }
            return;
        }
    }
}

static int pick_obj_at(designer_t* d, int sx, int sy) {
    if (!d) return -1;
    // top-most (last) wins
    for (int i = d->count - 1; i >= 0; i--) {
        design_obj_t* o = &d->objs[i];
        if (sx >= o->x && sx < o->x + o->w && sy >= o->y && sy < o->y + o->h) {
            return o->id;
        }
    }
    return -1;
}

static design_obj_t* add_obj(designer_t* d, design_obj_type_t t, int x, int y) {
    if (!d) return 0;
    if (d->count >= DESIGNER_MAX_OBJS) return 0;

    design_obj_t* o = &d->objs[d->count++];
    memset(o, 0, sizeof(*o));

    o->id = d->next_id++;
    o->type = t;

    if (t == DESIGN_OBJ_BUTTON) {
        o->w = DEF_BTN_W;
        o->h = DEF_BTN_H;
        o->color = 0xFF2D6CDF;
        strcpy(o->text, "Button");
    } else {
        o->w = DEF_LBL_W;
        o->h = DEF_LBL_H;
        o->color = 0xFF222222;
        strcpy(o->text, "Label");
    }

    // place centered at cursor (client coords)
    o->x = x - o->w / 2;
    o->y = y - o->h / 2;

    return o;
}

// ------------------------------
// drawing helpers
// ------------------------------
static void draw_panel_header(int x, int y, int w, const char* title) {
    gfx_fill_rect(x, y, w, TOP_H, 0xFFF0F0F0);
    gfx_draw_rect(x, y, w, TOP_H, 0xFFCCCCCC);
    gfx_draw_text_utf8(x + 10, y + 10, 0xFF333333, title);
}

static void draw_tool_item(int x, int y, int w, const char* text, bool selected) {
    uint32_t bg = selected ? 0xFF2D6CDF : 0xFFFFFFFF;
    uint32_t fg = selected ? 0xFFFFFFFF : 0xFF222222;

    gfx_fill_rect(x, y, w, ITEM_H, bg);
    gfx_draw_rect(x, y, w, ITEM_H, 0xFFCCCCCC);
    gfx_draw_text_utf8(x + 10, y + 8, fg, text);
}

static void draw_obj_preview(int ox, int oy, design_obj_t* o, bool selected) {
    if (!o) return;

    if (o->type == DESIGN_OBJ_BUTTON) {
        gfx_fill_rect(ox + o->x, oy + o->y, o->w, o->h, o->color);
        gfx_draw_rect(ox + o->x, oy + o->y, o->w, o->h, 0xFF000000);
        gfx_draw_text_utf8(ox + o->x + 8, oy + o->y + 7, 0xFFFFFFFF, o->text);
    } else { // label
        gfx_draw_text_utf8(ox + o->x, oy + o->y, o->color, o->text);
    }

    if (selected) {
        gfx_draw_rect(ox + o->x - 2, oy + o->y - 2, o->w + 4, o->h + 4, 0xFFFFA000);
    }
}

// ------------------------------
// app callbacks
// ------------------------------
static designer_t* st_of(app_t* app) {
    return (app && app->user) ? (designer_t*)app->user : 0;
}

static void designer_on_create(app_t* app) {
    designer_t* d = st_of(app);
    if (!d) return;

    memset(d, 0, sizeof(*d));
    d->count = 0;
    d->next_id = 1;
    d->tool = DESIGN_TOOL_NONE;
    d->selected_id = -1;
    d->dragging = 0;
    d->drag_id = -1;

    // ❌ app->wants_continuous_redraw yok (sende app struct’ında alan yok)
    // Continuous redraw’u şimdilik WM/desktop tarafında çöz.
}

static void designer_calc_preview(ui_rect_t c,
                                  int* out_center_x, int* out_center_w,
                                  int* out_preview_x, int* out_preview_y, int* out_preview_w, int* out_preview_h,
                                  int* out_client_x, int* out_client_y, int* out_client_w, int* out_client_h)
{
    int center_x = TOOLBOX_W;
    int center_w = c.w - TOOLBOX_W - PROPS_W;

    // clamp center_w
    if (center_w < 200) center_w = 200;

    // choose preview size but clamp into center
    int max_w = center_w - PREVIEW_MARGIN * 2;
    int max_h = c.h - (TOP_H + PREVIEW_MARGIN * 2);

    int pw = PREVIEW_DEF_W;
    int ph = PREVIEW_DEF_H;

    if (pw > max_w) pw = max_w;
    if (ph > max_h) ph = max_h;

    if (pw < 200) pw = 200;
    if (ph < 160) ph = 160;

    int px = center_x + (center_w - pw) / 2;
    int py = TOP_H + PREVIEW_MARGIN;

    int cx = px + PREVIEW_BORDER;
    int cy = py + PREVIEW_TITLE_H + PREVIEW_BORDER;
    int cw = pw - PREVIEW_BORDER * 2;
    int ch = ph - PREVIEW_TITLE_H - PREVIEW_BORDER * 2;

    if (cw < 1) cw = 1;
    if (ch < 1) ch = 1;

    if (out_center_x) *out_center_x = center_x;
    if (out_center_w) *out_center_w = center_w;

    if (out_preview_x) *out_preview_x = px;
    if (out_preview_y) *out_preview_y = py;
    if (out_preview_w) *out_preview_w = pw;
    if (out_preview_h) *out_preview_h = ph;

    if (out_client_x) *out_client_x = cx;
    if (out_client_y) *out_client_y = cy;
    if (out_client_w) *out_client_w = cw;
    if (out_client_h) *out_client_h = ch;
}

static void designer_on_draw(app_t* app) {
    designer_t* d = st_of(app);
    if (!d) return;

    ui_rect_t c = wm_get_client_rect(app->win_id);

    // Main bg
    gfx_fill_rect(0, 0, c.w, c.h, 0xFFFFFFFF);

    // Left toolbox area
    int toolbox_w = TOOLBOX_W;
    gfx_fill_rect(0, 0, toolbox_w, c.h, 0xFFF7F7F7);
    gfx_draw_rect(0, 0, toolbox_w, c.h, 0xFFCCCCCC);
    draw_panel_header(0, 0, toolbox_w, "Toolbox");

    int ty = TOP_H + 8;
    draw_tool_item(10, ty, toolbox_w - 20, "Button", (d->tool == DESIGN_TOOL_BUTTON));
    ty += ITEM_H + 8;
    draw_tool_item(10, ty, toolbox_w - 20, "Label", (d->tool == DESIGN_TOOL_LABEL));

    // Right properties area
    int props_w = PROPS_W;
    int props_x = c.w - props_w;

    gfx_fill_rect(props_x, 0, props_w, c.h, 0xFFF7F7F7);
    gfx_draw_rect(props_x, 0, props_w, c.h, 0xFFCCCCCC);
    draw_panel_header(props_x, 0, props_w, "Properties");

    // Center area background
    int center_x, center_w;
    int preview_x, preview_y, preview_w, preview_h;
    int client_x, client_y, client_w, client_h;

    designer_calc_preview(c,
        &center_x, &center_w,
        &preview_x, &preview_y, &preview_w, &preview_h,
        &client_x, &client_y, &client_w, &client_h);

    gfx_fill_rect(center_x, 0, center_w, c.h, 0xFFFAFAFA);
    gfx_draw_rect(center_x, 0, center_w, c.h, 0xFFDDDDDD);

    // Center header text
    gfx_draw_text_utf8(center_x + 12, 10, 0xFF333333, "Design Preview");

    // Draw preview window (form)
    gfx_fill_rect(preview_x, preview_y, preview_w, preview_h, 0xFFEFEFEF);
    gfx_draw_rect(preview_x, preview_y, preview_w, preview_h, 0xFF444444);

    // Titlebar
    gfx_fill_rect(preview_x + 1, preview_y + 1, preview_w - 2, PREVIEW_TITLE_H, 0xFF2D2D2D);
    gfx_draw_text_utf8(preview_x + 10, preview_y + 7, 0xFFFFFFFF, "Form1");

    // Client area
    gfx_fill_rect(client_x, client_y, client_w, client_h, 0xFFFFFFFF);
    gfx_draw_rect(client_x, client_y, client_w, client_h, 0xFFBDBDBD);

    // Draw objects inside client
    for (int i = 0; i < d->count; i++) {
        design_obj_t* o = &d->objs[i];
        bool sel = (o->id == d->selected_id);
        draw_obj_preview(client_x, client_y, o, sel);
    }

    // Properties content
    int py = TOP_H + 12;
    gfx_draw_text_utf8(props_x + 12, py, 0xFF333333, "Selected:");
    py += 20;

    if (d->selected_id <= 0) {
        gfx_draw_text_utf8(props_x + 12, py, 0xFF777777, "(none)");
        py += 18;
    } else {
        design_obj_t* o = find_obj_by_id(d, d->selected_id);
        if (o) {
            char buf[64];

            gfx_draw_text_utf8(props_x + 12, py, 0xFF444444, "id:");
            itoa_simple(o->id, buf, (int)sizeof(buf));
            gfx_draw_text_utf8(props_x + 60, py, 0xFF111111, buf);
            py += 18;

            gfx_draw_text_utf8(props_x + 12, py, 0xFF444444, "x:");
            itoa_simple(o->x, buf, (int)sizeof(buf));
            gfx_draw_text_utf8(props_x + 60, py, 0xFF111111, buf);
            py += 18;

            gfx_draw_text_utf8(props_x + 12, py, 0xFF444444, "y:");
            itoa_simple(o->y, buf, (int)sizeof(buf));
            gfx_draw_text_utf8(props_x + 60, py, 0xFF111111, buf);
            py += 18;

            gfx_draw_text_utf8(props_x + 12, py, 0xFF444444, "w:");
            itoa_simple(o->w, buf, (int)sizeof(buf));
            gfx_draw_text_utf8(props_x + 60, py, 0xFF111111, buf);
            py += 18;

            gfx_draw_text_utf8(props_x + 12, py, 0xFF444444, "h:");
            itoa_simple(o->h, buf, (int)sizeof(buf));
            gfx_draw_text_utf8(props_x + 60, py, 0xFF111111, buf);
            py += 18;

            gfx_draw_text_utf8(props_x + 12, py, 0xFF444444, "text:");
            gfx_draw_text_utf8(props_x + 12, py + 18, 0xFF111111, o->text);
            py += 44;
        }
    }

    // Hint bottom
    gfx_draw_text_utf8(center_x + 12, c.h - 18, 0xFF777777,
        "LMB: select/drag  |  Toolbox: place  |  ESC: tool none  |  DEL: delete selected");
}

static void designer_on_mouse(app_t* app, int mx, int my,
                              uint8_t pressed, uint8_t released, uint8_t buttons)
{
    (void)released;

    designer_t* d = st_of(app);
    if (!d) return;

    ui_rect_t c = wm_get_client_rect(app->win_id);

    int props_x = c.w - PROPS_W;
    int center_x, center_w;
    int preview_x, preview_y, preview_w, preview_h;
    int client_x, client_y, client_w, client_h;

    designer_calc_preview(c,
        &center_x, &center_w,
        &preview_x, &preview_y, &preview_w, &preview_h,
        &client_x, &client_y, &client_w, &client_h);

    // --- drag update (mouse move while holding) ---
    if (d->dragging && (buttons & 1)) {
        design_obj_t* o = find_obj_by_id(d, d->drag_id);
        if (o) {
            int sx = mx - client_x; // client-local
            int sy = my - client_y;

            o->x = sx - d->drag_off_x;
            o->y = sy - d->drag_off_y;

            // clamp into client
            if (o->x < 0) o->x = 0;
            if (o->y < 0) o->y = 0;
            if (o->x > client_w - o->w) o->x = client_w - o->w;
            if (o->y > client_h - o->h) o->y = client_h - o->h;
        }
        return;
    }

    // stop drag if button released
    if (d->dragging && !(buttons & 1)) {
        d->dragging = 0;
        d->drag_id = -1;
        return;
    }

    // only react to LMB press for clicks
    if (!(pressed & 1)) return;

    // --- Toolbox clicks ---
    if (mx < TOOLBOX_W) {
        int ty = TOP_H + 8;
        int item_x = 10;
        int item_w = TOOLBOX_W - 20;

        if (hit(mx, my, item_x, ty, item_w, ITEM_H)) {
            d->tool = DESIGN_TOOL_BUTTON;
            return;
        }
        ty += ITEM_H + 8;
        if (hit(mx, my, item_x, ty, item_w, ITEM_H)) {
            d->tool = DESIGN_TOOL_LABEL;
            return;
        }
        return;
    }

    // --- Properties clicks (ignore for now) ---
    if (mx >= props_x) {
        return;
    }

    // --- Click inside PREVIEW CLIENT area ---
    if (hit(mx, my, client_x, client_y, client_w, client_h)) {
        int sx = mx - client_x;
        int sy = my - client_y;

        // placing mode (toolbox selected)
        if (d->tool == DESIGN_TOOL_BUTTON || d->tool == DESIGN_TOOL_LABEL) {
            design_obj_type_t t = (d->tool == DESIGN_TOOL_BUTTON) ? DESIGN_OBJ_BUTTON : DESIGN_OBJ_LABEL;
            design_obj_t* o = add_obj(d, t, sx, sy);
            if (o) {
                d->selected_id = o->id;

                // immediately start dragging newly placed object
                d->dragging = 1;
                d->drag_id = o->id;
                d->drag_off_x = o->w / 2;
                d->drag_off_y = o->h / 2;

                // clamp right away
                if (o->x < 0) o->x = 0;
                if (o->y < 0) o->y = 0;
                if (o->x > client_w - o->w) o->x = client_w - o->w;
                if (o->y > client_h - o->h) o->y = client_h - o->h;
            }
            return;
        }

        // selection mode
        int id = pick_obj_at(d, sx, sy);
        d->selected_id = id;

        if (id > 0) {
            design_obj_t* o = find_obj_by_id(d, id);
            if (o) {
                d->dragging = 1;
                d->drag_id = id;
                d->drag_off_x = sx - o->x;
                d->drag_off_y = sy - o->y;
            }
        }
        return;
    }

    // click outside client -> deselect
    d->selected_id = -1;
}

static void designer_on_key(app_t* app, uint16_t key) {
    designer_t* d = st_of(app);
    if (!d) return;

    uint8_t sc = (uint8_t)(key & 0xFF);
    if (sc & 0x80) return; // ignore break

    // ESC: tool none
    if (sc == 0x01) {
        d->tool = DESIGN_TOOL_NONE;
        return;
    }

    // DEL (Set1): 0x53  (Backspace: 0x0E)
    if (sc == 0x53 || sc == 0x0E) {
        if (d->selected_id > 0) {
            remove_obj_by_id(d, d->selected_id);
        }
        return;
    }
}

static void designer_on_destroy(app_t* app) {
    (void)app;
}

const app_vtbl_t designer_vtbl = {
    .on_create  = designer_on_create,
    .on_draw    = designer_on_draw,
    .on_mouse   = designer_on_mouse,
    .on_key     = designer_on_key,
    .on_destroy = designer_on_destroy,

    // Sende app_vtbl içinde bunlar var; NULL bırakmak güvenli.
    .on_update = 0,
    .on_close_request = 0,
    .on_wheel = 0
};