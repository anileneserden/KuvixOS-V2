// ui/apps/demo_font.c
#include <app/app.h>
#include <ui/wm.h>
#include <kernel/drivers/video/gfx.h>
#include <ui/controls/label2.h>
#include <lib/string.h>
#include <stdint.h>
#include <stdbool.h>
#include <ui/apps/demo_font.h>

static demo_font_t* D(app_t* app) {
    return (app && app->user) ? (demo_font_t*)app->user : NULL;
}

static void demo_font_on_create(app_t* app) {
    demo_font_t* d = D(app);
    if (!d) return;

    // 0x24 -> '$' direkt byte test
    d->dollar_hex_str[0] = (char)0x24;
    d->dollar_hex_str[1] = '\0';

    // Label’lar
    ui_label2_init(&d->lbl_title, 100,
        (ui_point_t){ 16, 16 }, 0x00FF00,
        "Font Demo (label2 + gfx_draw_text_utf8)"
    );

    ui_label2_init(&d->lbl_1, 101,
        (ui_point_t){ 16, 40 }, 0x00FF00,
        "1) Normal string icinde $ :  DOLAR = $"
    );

    ui_label2_init(&d->lbl_2, 102,
        (ui_point_t){ 16, 60 }, 0x00FF00,
        "2) HEX 0x24 ile tek karakter: "
    );

    ui_label2_init(&d->lbl_3, 103,
        (ui_point_t){ 16, 80 }, 0x00FF00,
        "3) Turkce test: ğ Ğ ü Ü ş Ş ı İ ç Ç ö Ö é"
    );

    ui_label2_init(&d->lbl_4, 104,
        (ui_point_t){ 16, 100 }, 0x00FF00,
        "4) ASCII: !\"#$%&'()*+,-./ 0123456789"
    );

    ui_label2_init(&d->lbl_5, 105,
        (ui_point_t){ 16, 120 }, 0x00FF00,
        "5) Prompt testi: anil@kuvixos:~/Desktop$"
    );
}

static void demo_font_on_draw(app_t* app) {
    demo_font_t* d = D(app);
    if (!d) return;

    ui_rect_t client = wm_get_client_rect(app->win_id);

    // pencere içini temizle (origin client'a ayarlıysa 0,0 doğru)
    gfx_fill_rect(0, 0, client.w, client.h, 0x000000);

    // label2 draw çağır
    d->lbl_title.base.vtbl->draw(&d->lbl_title.base);
    d->lbl_1.base.vtbl->draw(&d->lbl_1.base);
    d->lbl_2.base.vtbl->draw(&d->lbl_2.base);

    // lbl_2'nin yanına 0x24 string'i yaz
    // lbl_2 metni 16,60’ta bitiyor olabilir; güvenli olsun diye direkt x=280 gibi koy
    gfx_draw_text_utf8(280, 60, 0x00FF00, d->dollar_hex_str);

    d->lbl_3.base.vtbl->draw(&d->lbl_3.base);
    d->lbl_4.base.vtbl->draw(&d->lbl_4.base);
    d->lbl_5.base.vtbl->draw(&d->lbl_5.base);

    // Ek: tek karakteri raw byte ile de çizelim (çok net teşhis)
    {
        char raw[2];
        raw[0] = (char)0x24;
        raw[1] = 0;
        gfx_draw_text_utf8(16, 150, 0x00FF00, "RAW 0x24 => ");
        gfx_draw_text_utf8(16 + (12 * 8), 150, 0x00FF00, raw);
    }
}

static void demo_font_on_mouse(app_t* app, int mx, int my,
                               uint8_t pressed, uint8_t released, uint8_t buttons) {
    (void)app; (void)mx; (void)my; (void)pressed; (void)released; (void)buttons;
}

static void demo_font_on_key(app_t* app, uint16_t key) {
    (void)app; (void)key;
}

static void demo_font_on_destroy(app_t* app) {
    (void)app;
}

const app_vtbl_t demo_font_vtbl = {
    .on_create  = demo_font_on_create,
    .on_draw    = demo_font_on_draw,
    .on_mouse   = demo_font_on_mouse,
    .on_key     = demo_font_on_key,
    .on_destroy = demo_font_on_destroy
};