#include <ui/controls/ui_context.h>
#include <lib/string.h>

// ------------------------------------------------------------
// Recursive hit-test: en derin child (üstte olan önce)
// Not: child z-order'un yoksa eklenen sıraya göre "son eklenen üstte" varsayımı yapar.
// ------------------------------------------------------------
static ui_control_t* hittest_control(ui_control_t* c, int mx, int my) {
    if (!c || !c->visible || !c->enabled) return 0;

    // Önce çocuklar: son sibling en üstte gibi kabul edelim
    // Bunun için child'ları stack'e alıp ters dolaşabiliriz,
    // ama basit çözüm: recursion ile önce child içine gir, sibling'ları sonra dolaş.
    // Daha iyi: child listesini geçici diziye koyup tersten gez.
    // MVP için: child listesi ekleme sırasına göre zaten üstte kalıyorsa yeter.

    // Çocuklarda arama
    // (Daha iyi "sondan başa" için küçük bir buffer kullanıyoruz)
    ui_control_t* arr[128];
    int n = 0;
    for (ui_control_t* ch = c->first_child; ch && n < 128; ch = ch->next_sibling) {
        arr[n++] = ch;
    }
    for (int i = n - 1; i >= 0; --i) {
        ui_control_t* hit = hittest_control(arr[i], mx, my);
        if (hit) return hit;
    }

    // Sonra kendisi
    if (ui_control_contains(c, mx, my)) return c;
    return 0;
}

static ui_control_t* ui_ctx_hittest(ui_context_t* ui, int mx, int my) {
    if (!ui) return 0;

    // Root'lar için de "son eklenen üstte" gibi gez
    for (int i = ui->root_count - 1; i >= 0; --i) {
        ui_control_t* r = ui->roots[i];
        ui_control_t* hit = hittest_control(r, mx, my);
        if (hit) return hit;
    }
    return 0;
}

// ------------------------------------------------------------
// Event dispatch helper
// ------------------------------------------------------------
static void dispatch_event(ui_control_t* c, ui_event_type_t type, int mx, int my) {
    if (!c || !c->vtbl || !c->vtbl->handle_event) return;

    ui_event_t e;
    e.type = type;
    e.mouse_x = mx;
    e.mouse_y = my;
    e.mouse_button = 0;
    e.key = 0;

    c->vtbl->handle_event(c, &e);
}

// ------------------------------------------------------------
// Public API
// ------------------------------------------------------------
void ui_ctx_init(ui_context_t* ui) {    
    if (!ui) return;
    memset(ui, 0, sizeof(*ui));
    ui->theme = ui_get_theme();
    ui->root_count = 0;

    ui->mouse_x = 0;
    ui->mouse_y = 0;
    ui->mouse_down = false;

    ui->hot = 0;
    ui->active = 0;

    ui->has_dirty = true;
}

bool ui_ctx_add_root(ui_context_t* ui, ui_control_t* c) {
    if (!ui || !c) return false;
    if (ui->root_count >= UI_MAX_ROOTS) return false;
    ui->roots[ui->root_count++] = c;
    ui->has_dirty = true;
    return true;
}

void ui_ctx_draw(ui_context_t* ui) {
    if (!ui) return;

    // root'ları sırayla çiz
    for (int i = 0; i < ui->root_count; ++i) {
        ui_control_t* r = ui->roots[i];
        if (!r || !r->visible) continue;
        if (r->vtbl && r->vtbl->draw) r->vtbl->draw(r);
    }

    ui->has_dirty = false;
}

void ui_ctx_mouse(ui_context_t* ui, int mx, int my, bool left_down) {
    if (!ui) return;

    ui->mouse_x = mx;
    ui->mouse_y = my;

    ui_control_t* hit = ui_ctx_hittest(ui, mx, my);

    // HOT update
    if (hit != ui->hot) {
        if (ui->hot) ui->hot->hovered = false;
        ui->hot = hit;
        if (ui->hot) ui->hot->hovered = true;
        ui->has_dirty = true;
    }

    bool was_down = ui->mouse_down;
    ui->mouse_down = left_down;

    // Mouse Down edge
    if (left_down && !was_down) {
        if (ui->active) ui->active->pressed = false; // güvenlik
        ui->active = hit;
        if (ui->active) {
            ui->active->pressed = true;
            dispatch_event(ui->active, UI_EVT_MOUSE_DOWN, mx, my);
        }
        ui->has_dirty = true;
        return;
    }

    // Mouse Up edge  ✅ aradığın pressed reset burada
    if (!left_down && was_down) {
        ui_control_t* cap = ui->active;
        ui->active = 0;

        if (cap) {
            cap->pressed = false;                 // 🔥 her durumda bırak
            dispatch_event(cap, UI_EVT_MOUSE_UP, mx, my);
        }

        // Click yalnızca aynı kontrol üstünde bırakıldıysa
        if (cap && cap == hit) {
            dispatch_event(cap, UI_EVT_CLICK, mx, my);
        }

        ui->has_dirty = true;
        return;
    }

    // move event istersen:
    // dispatch_event(hit, UI_EVT_MOUSE_MOVE, mx, my);
}
