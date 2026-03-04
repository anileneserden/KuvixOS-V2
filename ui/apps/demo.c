// kernel/ui/apps/demo_app.c

#include <app/app.h>
#include <ui/wm.h>
#include <kernel/drivers/video/gfx.h>
#include <ui/dialogs/messagebox.h>
#include <stdint.h>
#include <stdbool.h>

#include <ui/controls/ui_context.h>
#include <ui/controls/panel2.h>
#include <ui/controls/label2.h>
#include <ui/controls/button2.h>

typedef struct {
    int window_id;

    ui_context_t ui;

    ui_panel2_t  root;
    ui_label2_t  lbl;
    ui_button2_t btn;
    ui_button2_t btn_msg;

    int  counter;
    char buf[32];
} demo_app_t;

static void itoa_simple(int v, char* out) {
    char tmp[16];
    int n = 0;

    if (v == 0) { out[0] = '0'; out[1] = 0; return; }

    if (v < 0) { *out++ = '-'; v = -v; }

    while (v > 0 && n < (int)sizeof(tmp)) {
        tmp[n++] = (char)('0' + (v % 10));
        v /= 10;
    }

    for (int i = 0; i < n; i++) out[i] = tmp[n - 1 - i];
    out[n] = 0;
}

static void on_click_inc(void* user) {
    demo_app_t* d = (demo_app_t*)user;
    d->counter++;
    itoa_simple(d->counter, d->buf);
    ui_label2_set_text(&d->lbl, d->buf);

    // UI değişti -> redraw gerekebilir (senin sistemde mouse event zaten full present yapıyor)
    // İstersen burada WM'e "redraw" flag’i koyarsın.
}

static void on_click_msg(void* user) {
    (void)user;
    MessageBox.Show(
        "KuvixOS",
        "Bu bir test MessageBox.",
        MB_ICON_INFO,
        MessageBoxButtons.OK
    );
}

static void demo_layout(demo_app_t* d) {
    // WM origin set ettiği için: burada sadece size alacağız.
    // Client rect'in w/h lazım.
    ui_rect_t c = wm_get_client_rect(d->window_id);

    // Root panel client alanını kaplasın (koordinat: client relative)
    d->root.base.location = (ui_point_t){ 0, 0 };
    d->root.base.size     = (ui_size_t){ c.w, c.h };

    // Child'lar da client relative
    d->lbl.base.location  = (ui_point_t){ 12, 12 };

    d->btn.base.location  = (ui_point_t){ 12, 44 };
    d->btn.base.size      = (ui_size_t){ 90, 26 };

    d->btn_msg.base.location = (ui_point_t){ 110, 44 };
    d->btn_msg.base.size     = (ui_size_t){ 130, 26 };
}

static void demo_on_create(app_t* self) {
    demo_app_t* d = (demo_app_t*)self->user;
    d->window_id = self->win_id;

    ui_ctx_init(&d->ui);

    d->counter = 0;
    itoa_simple(0, d->buf);

    ui_panel2_init(&d->root,  1, (ui_point_t){0,0}, (ui_size_t){0,0}, 0xFFFFFF);
    ui_panel2_set_border(&d->root, 1, 0xC0C0C0);

    ui_label2_init(&d->lbl,  2, (ui_point_t){0,0}, 0x000000, d->buf);

    ui_button2_init(&d->btn, 3, (ui_point_t){0,0}, (ui_size_t){90,26}, "+1");
    ui_button2_onclick(&d->btn, on_click_inc, d);

    ui_button2_init(&d->btn_msg, 4, (ui_point_t){0,0}, (ui_size_t){130,26}, "MessageBox");
    ui_button2_onclick(&d->btn_msg, on_click_msg, d);

    // root -> children
    ui_control_add_child(&d->root.base, &d->lbl.base);
    ui_control_add_child(&d->root.base, &d->btn.base);
    ui_control_add_child(&d->root.base, &d->btn_msg.base);

    // ui roots
    ui_ctx_add_root(&d->ui, &d->root.base);

    demo_layout(d);
}

static void demo_on_draw(app_t* self) {
    demo_app_t* d = (demo_app_t*)self->user;

    // WM zaten origin set ediyor, layout'u güncelle
    demo_layout(d);

    // ❌ burada wm_get_client_rect ile fill yapma!
    // Panel zaten background çiziyor.
    ui_ctx_draw(&d->ui);
}

static void demo_on_mouse(app_t* self,
                          int mx, int my,
                          uint8_t pressed,
                          uint8_t released,
                          uint8_t buttons)
{
    (void)pressed;
    (void)released;

    demo_app_t* d = (demo_app_t*)self->user;

    if (messagebox_is_visible()) return;
    if (wm_is_any_window_captured()) return;

    // mx,my zaten client coords geliyor
    bool ldown = (buttons & 1) != 0;
    ui_ctx_mouse(&d->ui, mx, my, ldown);
}

static void demo_on_key(app_t* self, uint16_t sc) { (void)self; (void)sc; }
static void demo_on_destroy(app_t* self) { (void)self; }

const app_vtbl_t demo_app_vtbl = {
    .on_create = demo_on_create,
    .on_draw   = demo_on_draw,
    .on_key    = demo_on_key,
    .on_mouse  = demo_on_mouse,
    .on_destroy= demo_on_destroy,
    .on_close_request = 0
};
