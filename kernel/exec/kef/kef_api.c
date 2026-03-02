#include <kernel/exec/kef_api.h>

#include <kernel/printk.h>
#include <kernel/drivers/video/gfx.h>
#include <ui/desktop.h>

#include <ui/apps/kef_host.h>   // ✅ kef_host_get_active + widgets
#include <lib/string.h>
#include <ui/wm.h>

#include <ui/theme.h>

extern uint32_t g_ticks_ms;

static int g_kef_active = 0;

// ----------------- core api -----------------
static void api_log_impl(const char* s) {
    if (!s) return;
    printk("%s", s);
}

static void api_fill_rect_impl(int x, int y, int w, int h, uint32_t color) {
    gfx_fill_rect(x, y, w, h, color);
}

static void api_text_impl(int x, int y, uint32_t color, const char* s) {
    if (!s) return;
    gfx_draw_text_utf8(x, y, color, s);
}

static void api_invalidate_impl(void) {
    static uint32_t last = 0;
    uint32_t now = g_ticks_ms;
    if (now == last) return;
    last = now;
    desktop_invalidate_full();
}

static void api_set_window_size_impl(int client_w, int client_h) {
    kef_host_t* host = kef_host_get_active();
    if (!host) return;
    if (host->win_id < 0) return;

    const ui_theme_t* th = ui_get_theme();
    int border  = 2;
    int title_h = 24;

    if (th) {
        border  = th->window_border_px;
        title_h = th->window_title_h;
        if (border < 1) border = 1;
        if (title_h < 18) title_h = 18;
    }

    // ✅ client size -> window size dönüşümü
    int win_w = client_w + (border * 2);
    int win_h = client_h + title_h + (border * 2);

    printk("[KEF] set_window_size win_id=%d client=%dx%d -> win=%dx%d (border=%d title=%d)\n",
           host->win_id, client_w, client_h, win_w, win_h, border, title_h);

    wm_set_window_size(host->win_id, win_w, win_h);

    // eski alanı temizlemek için
    desktop_invalidate_full();
}

// ----------------- widget create api -----------------
static void api_create_label_impl(int x, int y, const char* text) {
    kef_host_t* h = kef_host_get_active();
    if (!h) return;
    if (h->widget_count >= KEF_MAX_WIDGETS) return;

    widget_t* w = &h->widgets[h->widget_count++];
    memset(w, 0, sizeof(*w));

    w->type = WIDGET_LABEL;
    w->x = x;
    w->y = y;
    w->visible = 1;

    if (text)
        strncpy(w->text, text, sizeof(w->text) - 1);
}

static void api_create_button_impl(int x, int y, int w, int h,
                                   const char* text,
                                   void (*on_click)(void*),
                                   void* user) {
    kef_host_t* host = kef_host_get_active();
    if (!host) return;
    if (host->widget_count >= KEF_MAX_WIDGETS) return;

    widget_t* wd = &host->widgets[host->widget_count++];
    memset(wd, 0, sizeof(*wd));

    wd->type = WIDGET_BUTTON;
    wd->x = x;
    wd->y = y;
    wd->w = w;
    wd->h = h;
    wd->visible = 1;

    if (text)
        strncpy(wd->text, text, sizeof(wd->text) - 1);

    wd->on_click = on_click;
    wd->user = user;
}

// ----------------- export api -----------------
const kvx_api_t g_kvx_api = {
    .log             = api_log_impl,
    .fill_rect       = api_fill_rect_impl,
    .text            = api_text_impl,
    .invalidate      = api_invalidate_impl,
    .create_label    = api_create_label_impl,
    .create_button   = api_create_button_impl,
    .set_window_size = api_set_window_size_impl,
};

void kef_api_set_active(int on) { g_kef_active = on ? 1 : 0; }
int  kef_api_is_active(void)    { return g_kef_active; }