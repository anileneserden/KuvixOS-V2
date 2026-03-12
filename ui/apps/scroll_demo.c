// ui/apps/scroll_demo.c
#include <ui/apps/scroll_demo.h>

#include <app/app.h>
#include <ui/wm.h>
#include <kernel/drivers/video/gfx.h>
#include <lib/string.h>
#include <stdint.h>
#include <stdbool.h>

static scroll_demo_t* SD(app_t* app) { return (scroll_demo_t*)app->user; }

static int clampi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void fill_rect_clipped(int x, int y, int w, int h,
                              int cx, int cy, int cw, int ch,
                              uint32_t color)
{
    int x0 = x;
    int y0 = y;
    int x1 = x + w;
    int y1 = y + h;

    int cx0 = cx;
    int cy0 = cy;
    int cx1 = cx + cw;
    int cy1 = cy + ch;

    if (x0 < cx0) x0 = cx0;
    if (y0 < cy0) y0 = cy0;
    if (x1 > cx1) x1 = cx1;
    if (y1 > cy1) y1 = cy1;

    int nw = x1 - x0;
    int nh = y1 - y0;
    if (nw <= 0 || nh <= 0) return;

    gfx_fill_rect(x0, y0, nw, nh, color);
}

static void draw_rect_clipped(int x, int y, int w, int h,
                              int cx, int cy, int cw, int ch,
                              uint32_t color)
{
    // Basit: sınırlar clip içinde kalıyorsa çiz, yoksa hiç çizme (yeterli)
    if (x < cx || y < cy || (x + w) > (cx + cw) || (y + h) > (cy + ch))
        return;
    gfx_draw_rect(x, y, w, h, color);
}

static bool in_rect(int x, int y, int rx, int ry, int rw, int rh) {
    return (x >= rx && x < (rx + rw) && y >= ry && y < (ry + rh));
}

static int max_scroll(const scroll_demo_t* s) {
    int m = s->content_h - s->vh;
    return (m > 0) ? m : 0;
}

static void apply_wheel(scroll_demo_t* s, int wheel_step) {
    // wheel_step: +1/-1
    // Yukarı scroll genelde içerik yukarı çıkar -> offset azalır.
    // tersse burayı + yaparsın.
    s->offset_y -= wheel_step * s->wheel_px;

    int m = max_scroll(s);
    s->offset_y = clampi(s->offset_y, 0, m);
}

static void draw_scroll_area(const scroll_demo_t* s) {
    // viewport background (dark gray)
    gfx_fill_rect(s->vx, s->vy, s->vw, s->vh, 0x00303030);
    gfx_draw_rect(s->vx, s->vy, s->vw, s->vh, 0x00606060);

    // content box (light gray) - uzun bir kutu düşün
    // İçerik, viewport içinde sanki yukarı/aşağı kayıyormuş gibi çizilecek.
    int content_x = s->vx + 8;
    int content_w = s->vw - 16;

    int content_y0 = s->vy + 8 - s->offset_y; // kaydırma burada

    // İçeriği parça parça çizelim: örnek “satırlar”
    // (Clip yok, o yüzden sadece görünen satırları çiziyoruz)
    const int row_h = 28;
    int rows = (s->content_h / row_h) + 1;

    // ilk görünür row index
    int first = (s->offset_y / row_h);
    if (first < 0) first = 0;

    // ekranda kaç row olabilir
    int max_vis = (s->vh / row_h) + 3;

    int last = first + max_vis;
    if (last > rows) last = rows;

    for (int i = first; i < last; i++) {
        int y = content_y0 + i * row_h;
        int box_h = row_h - 4;

        // ✅ tamamen viewport üstündeyse geç
        if (y + box_h < s->vy) continue;
        // ✅ tamamen viewport altına çıktıysa bitir
        if (y >= s->vy + s->vh) break;

        int clip_x = s->vx;
        int clip_y = s->vy;
        int clip_w = s->vw;
        int clip_h = s->vh;

        // ✅ satır kutusu (clipped)
        fill_rect_clipped(content_x, y, content_w, box_h,
                        clip_x, clip_y, clip_w, clip_h,
                        0x00B0B0B0);

        // border: tamamen içerideyse çiz (istersen sonra geliştiririz)
        draw_rect_clipped(content_x, y, content_w, box_h,
                        clip_x, clip_y, clip_w, clip_h,
                        0x00808080);

        // ✅ "Item N" label üret
        char buf[64];
        int p = 0;
        const char* pre = "Item ";
        while (pre[p] && p < 63) { buf[p] = pre[p]; p++; }

        int n = i;
        char tmp[16];
        int ti = 0;

        if (n == 0) {
            tmp[ti++] = '0';
        } else {
            char rev[16];
            int ri = 0;
            while (n > 0 && ri < 15) { rev[ri++] = (char)('0' + (n % 10)); n /= 10; }
            while (ri > 0 && ti < 15) tmp[ti++] = rev[--ri];
        }
        tmp[ti] = 0;

        for (int k = 0; tmp[k] && p < 63; k++) buf[p++] = tmp[k];
        buf[p] = 0;

        // ✅ text: sadece text baseline viewport içindeyse çiz
        int ty = y + 6;
        if (ty >= clip_y && ty <= (clip_y + clip_h - 10)) {
            gfx_draw_text_utf8(content_x + 8, ty, 0x00101010, buf);
        }
    }

    // scrollbar (sağda)
    int m = max_scroll(s);
    if (m > 0) {
        int bar_x = s->vx + s->vw - 10;
        int bar_y = s->vy + 2;
        int bar_w = 8;
        int bar_h = s->vh - 4;

        // track
        gfx_fill_rect(bar_x, bar_y, bar_w, bar_h, 0x00222222);

        // thumb
        int thumb_h = (s->vh * bar_h) / s->content_h;
        if (thumb_h < 14) thumb_h = 14;
        if (thumb_h > bar_h) thumb_h = bar_h;

        int thumb_y = bar_y + (s->offset_y * (bar_h - thumb_h)) / m;

        gfx_fill_rect(bar_x, thumb_y, bar_w, thumb_h, 0x00999999);
    }
}

static void scroll_demo_on_create(app_t* app) {
    scroll_demo_t* s = SD(app);
    if (!s) return;

    // default values
    s->vx = 30;
    s->vy = 100;
    s->vw = 340;
    s->vh = 240;

    s->content_h = 700;  // içerik daha uzun
    s->offset_y  = 0;
    s->wheel_px  = 32;

    s->mx = 0;
    s->my = 0;
}

static void scroll_demo_on_draw(app_t* app) {
    scroll_demo_t* s = SD(app);
    if (!s) return;

    ui_rect_t cr = wm_get_client_rect(app->win_id);

    // arka plan
    gfx_fill_rect(0, 0, cr.w, cr.h, 0x00101010);

    // başlık
    gfx_draw_text_utf8(12, 10, 0x00FFFFFF, "Scroll Demo: hover box + wheel to scroll");

    // viewport’u pencere boyuna göre istersen ayarla:
    // (şimdilik sabit kalsın, ama taşarsa clamp)
    if (s->vx + s->vw > cr.w) s->vw = cr.w - s->vx - 8;
    if (s->vy + s->vh > cr.h) s->vh = cr.h - s->vy - 8;
    if (s->vw < 80) s->vw = 80;
    if (s->vh < 80) s->vh = 80;

    // debug info
    // offset/max
    char info[96];
    // basit string
    // "offset: X / max: Y"
    // hızlıca yaz:
    int p = 0;
    const char* a = "offset: ";
    while (a[p]) { info[p] = a[p]; p++; }
    // offset int
    {
        int v = s->offset_y;
        char tmp[16]; int ti=0;
        if (v==0) tmp[ti++]='0';
        else { char rev[16]; int ri=0; while(v>0 && ri<15){rev[ri++]=(char)('0'+(v%10)); v/=10;} while(ri>0) tmp[ti++]=rev[--ri]; }
        tmp[ti]=0;
        for(int k=0; tmp[k] && p<95; k++) info[p++]=tmp[k];
    }
    const char* b = " / max: ";
    for (int k=0; b[k] && p<95; k++) info[p++] = b[k];
    {
        int v = max_scroll(s);
        char tmp[16]; int ti=0;
        if (v==0) tmp[ti++]='0';
        else { char rev[16]; int ri=0; while(v>0 && ri<15){rev[ri++]=(char)('0'+(v%10)); v/=10;} while(ri>0) tmp[ti++]=rev[--ri]; }
        tmp[ti]=0;
        for(int k=0; tmp[k] && p<95; k++) info[p++]=tmp[k];
    }
    info[p]=0;
    gfx_draw_text_utf8(12, 28, 0x00AAAAAA, info);

    // scroll area
    draw_scroll_area(s);

    // hover indicator
    bool hover = in_rect(s->mx, s->my, s->vx, s->vy, s->vw, s->vh);
    gfx_draw_text_utf8(12, 46, hover ? 0x00FFFF00 : 0x00666666,
                       hover ? "HOVER: wheel active" : "HOVER: move mouse into box");
}

static void scroll_demo_on_mouse(app_t* app, int mx, int my,
                                 uint8_t pressed, uint8_t released, uint8_t buttons) {
    (void)pressed; (void)released; (void)buttons;
    scroll_demo_t* s = SD(app);
    if (!s) return;

    // WM zaten client coords gönderiyor (sen düzeltmiştin)
    s->mx = mx;
    s->my = my;
}

static void scroll_demo_on_wheel(app_t* app, int wheel_step) {
    scroll_demo_t* s = SD(app);
    if (!s) return;

    // sadece mouse viewport içindeyken scroll
    if (!in_rect(s->mx, s->my, s->vx, s->vy, s->vw, s->vh))
        return;

    apply_wheel(s, wheel_step);
}

static void scroll_demo_on_destroy(app_t* app) { (void)app; }

const app_vtbl_t scroll_demo_vtbl = {
    .on_create  = scroll_demo_on_create,
    .on_draw    = scroll_demo_on_draw,
    .on_mouse   = scroll_demo_on_mouse,
    .on_key     = 0,
    .on_destroy = scroll_demo_on_destroy,
    .on_wheel   = scroll_demo_on_wheel,
    .on_close_request = 0,
};