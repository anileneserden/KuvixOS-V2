#include <lib/game_engine.h>
#include <app/app.h>
#include <ui/wm.h>
#include <kernel/time.h>
#include <kernel/drivers/video/gfx.h>

static game_object_t* player = 0;
static uint32_t last_frame_ms = 0;

void kuvix_start() {
    player = engine_create_object(50, 50, 32, 32, 0x00FF00);
}

void game_on_draw(app_t* app) {
    // Null kontrolü kritik! kmalloc başarısız olduysa çöker.
    if (!app || !player) return;

    // 1. ZAMAN HESABI (Milisaniye / Integer)
    uint32_t current_ms = g_ticks_ms;
    if (last_frame_ms == 0) {
        last_frame_ms = current_ms;
        return;
    }
    uint32_t dt_ms = current_ms - last_frame_ms;
    last_frame_ms = current_ms;

    // Hız saniyede 300 piksel ise: (300 * dt_ms) / 1000
    // Örn: 16ms geçtiyse -> (300 * 16) / 1000 = 4 piksel hareket.
    int speed = 300;
    int move = (speed * (int)dt_ms) / 1000;

    // 2. INPUT (Sinyal varsa hareket et)
    if (move > 0) {
        if (Input.GetKey(0x11)) player->y -= move; // W
        if (Input.GetKey(31))   player->y += move; // S
        if (Input.GetKey(30))   player->x -= move; // A
        if (Input.GetKey(32))   player->x += move; // D
    }

    // 3. SINIRLAR
    if (player->x < 0) player->x = 0;
    if (player->y < 0) player->y = 0;
    if (player->x > app->width - player->w) player->x = app->width - player->w;
    if (player->y > app->height - player->h) player->y = app->height - player->h;

    // 4. ÇİZİM
    // ÖNEMLİ: wm_draw() çağrısını buraya koyma! 
    // Masaüstü döngüsü (desktop.c) zaten çizimi tetikleyecektir.
    // wm_draw() burada kendi kendini sonsuz kere çağırıp stack'i patlatabilir (Reset sebebi!)
    
    gfx_fill_rect(0, 0, app->width, app->height, 0x1A1A1A);
    engine_loop();

    wm_draw();
}

void game_on_key(app_t* app, uint16_t key) {
    (void)app;
    // Desktop.c zaten engine_internal_set_key çağırıyor olmalı.
}

void game_on_create(app_t* app) {
    engine_setup(app->win_id);
}

const app_vtbl_t game_engine_vtbl = {
    .on_create = game_on_create,
    .on_draw = game_on_draw,
    .on_key = game_on_key,
};