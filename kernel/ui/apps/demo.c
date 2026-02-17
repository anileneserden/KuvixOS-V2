#include <ui/apps/demo.h>
#include <ui/ui_label.h>
#include <kernel/drivers/video/gfx.h>
#include <app/app.h>

static void demo_on_draw(app_t* a) {
    (void)a;

    ui_label_t l1 = { 20, 20, 0x00000000, "Demo App - Türkçe Test" };
    ui_label_t l2 = { 20, 40, 0x00000000, "Masaüstü  |  Çöp Kutusu" };
    ui_label_t l3 = { 20, 60, 0x00000000, "ı Ğ ğ Ü ü Ş ş İ i Ö ö Ç ç" };
    ui_label_t l4 = { 20, 80, 0x00000000, "Pijamalı hasta yağız şoföre çabucak güvendi." };

    ui_label_draw(&l1);
    ui_label_draw(&l2);
    ui_label_draw(&l3);
    ui_label_draw(&l4);
}

const app_vtbl_t demo_vtbl = {
    .on_create = 0,
    .on_draw   = demo_on_draw,
    .on_key    = 0,
    .on_update = 0
};
