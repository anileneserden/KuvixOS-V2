#include <ui/apps/physics_lab.h>

#include <app/app.h>
#include <ui/wm.h>

#include <kernel/drivers/video/gfx.h>
#include <kernel/time.h>
#include <lib/string.h>
#include <stdint.h>
#include <stdbool.h>

// tick
extern volatile uint32_t g_ticks_ms;

// scancodes (Set1)
#define SC_A     0x1E
#define SC_D     0x20
#define SC_W     0x11
#define SC_SPACE 0x39

// ------------------------------
// 16.16 fixed-point helpers
// ------------------------------
#define FX_SHIFT 16
#define FX_ONE   (1 << FX_SHIFT)

static inline fx fx_from_int(int v) { return (fx)(v << FX_SHIFT); }
static inline int fx_to_int(fx v)   { return (int)(v >> FX_SHIFT); }

static inline fx fx_mul(fx a, fx b) {
    return (fx)(((int64_t)a * (int64_t)b) >> FX_SHIFT);
}
static inline fx fx_div(fx a, fx b) {
    return (fx)(((int64_t)a << FX_SHIFT) / (int64_t)b);
}

// dt scaling: value_per_16ms * dt/16
static inline fx fx_scale_dt(fx v_per_16ms, int dt_ms) {
    // v * dt / 16
    return (fx)(((int64_t)v_per_16ms * (int64_t)dt_ms) / 16);
}

static physics_lab_t* st_of(app_t* app) {
    return (app && app->user) ? (physics_lab_t*)app->user : NULL;
}

static void physics_lab_reset(physics_lab_t* s, int client_w, int client_h) {
    (void)client_w;

    s->playerW = 40;
    s->playerH = 40;

    s->posX = fx_from_int(40);
    s->posY = fx_from_int(client_h / 2);

    s->velX = 0;
    s->velY = 0;

    // -------- tuning --------
    // gravity: 0.60 per 16ms (yaklaşık)
    // jumpImpulse: -12.00 (impulse)
    s->gravity     = fx_div(fx_from_int(3), fx_from_int(5));   // 3/5 = 0.60
    s->jumpImpulse = fx_from_int(-12);

    // acceleration (x): 1.40 per 16ms
    // max velocity: 10.00
    s->accelX  = 14; // 14/10 gibi düşün (aşağıda fx'e çevireceğiz)
    s->maxVelX = 10;

    // friction: 0.50 per 16ms (yaklaşık)
    s->friction = 1; // basit integer; aşağıda fixed uygulanıyor

    s->grounded = 0;

    memset(s->key_down, 0, sizeof(s->key_down));
    memset(s->key_prev, 0, sizeof(s->key_prev));
}

static void physics_lab_on_create(app_t* app) {
    physics_lab_t* s = st_of(app);
    if (!s) return;

    // bu app sürekli redraw istiyor
    app->wants_continuous_redraw = 1;

    memset(s, 0, sizeof(*s));
    s->last_ms = (uint32_t)g_ticks_ms;

    ui_rect_t c = wm_get_client_rect(app->win_id);
    physics_lab_reset(s, c.w, c.h);
}

static void physics_lab_on_draw(app_t* app) {
    physics_lab_t* s = st_of(app);
    if (!s) return;

    ui_rect_t c = wm_get_client_rect(app->win_id);

    // -------- dt --------
    uint32_t now = (uint32_t)g_ticks_ms;
    int dt = (int)(now - s->last_ms);
    s->last_ms = now;

    if (dt <= 0) dt = 1;
    if (dt > 50) dt = 50;

    // ground (client area içinde)
    int groundY_i = c.h - 30;
    fx groundY = fx_from_int(groundY_i);

    // -------- input --------
    bool left  = s->key_down[SC_A] != 0;
    bool right = s->key_down[SC_D] != 0;

    // jump EDGE trigger: sadece yeni basışta zıplasın
    bool jump_down = (s->key_down[SC_W] || s->key_down[SC_SPACE]);
    bool jump_prev = (s->key_prev[SC_W] || s->key_prev[SC_SPACE]);
    bool jump_pressed = (jump_down && !jump_prev);

    // grounded kontrol
    // posY >= groundY - playerH
    fx playerH = fx_from_int(s->playerH);
    s->grounded = (s->posY >= (groundY - playerH)) ? 1 : 0;

    // -------- physics update --------
    // X acceleration
    fx accel = fx_div(fx_from_int(s->accelX), fx_from_int(10)); // 14 -> 1.4
    fx maxV  = fx_from_int(s->maxVelX);

    if (left) {
        s->velX -= fx_scale_dt(accel, dt);
        if (s->velX < -maxV) s->velX = -maxV;
    } else if (right) {
        s->velX += fx_scale_dt(accel, dt);
        if (s->velX > maxV) s->velX = maxV;
    } else {
        // friction: velX'i 0'a yaklaştır
        // 0.50 per 16ms gibi bir etki verelim
        fx fr = fx_div(fx_from_int(1), fx_from_int(2)); // 0.5
        fx d  = fx_scale_dt(fr, dt);

        if (s->velX > 0) {
            s->velX -= d;
            if (s->velX < 0) s->velX = 0;
        } else if (s->velX < 0) {
            s->velX += d;
            if (s->velX > 0) s->velX = 0;
        }
    }

    // Jump: sadece grounded + edge
    if (jump_pressed && s->grounded) {
        s->velY = s->jumpImpulse;
        s->grounded = 0;
    }

    // gravity
    s->velY += fx_scale_dt(s->gravity, dt);

    // integrate
    s->posX += fx_scale_dt(s->velX, dt);
    s->posY += fx_scale_dt(s->velY, dt);

    // -------- collisions --------
    // ground clamp
    if (s->posY >= (groundY - playerH)) {
        s->posY = (groundY - playerH);
        if (s->velY > 0) s->velY = 0;
        s->grounded = 1;
    }

    // walls clamp
    int maxX_i = c.w - s->playerW;
    if (maxX_i < 0) maxX_i = 0;

    fx maxX = fx_from_int(maxX_i);

    if (s->posX < 0) s->posX = 0;
    if (s->posX > maxX) s->posX = maxX;

    // ceiling clamp
    if (s->posY < 0) {
        s->posY = 0;
        if (s->velY < 0) s->velY = 0;
    }

    // -------- render --------
    gfx_fill_rect(0, 0, c.w, c.h, 0xFF050505);

    // ground line
    gfx_fill_rect(0, groundY_i, c.w, 4, 0xFFFF0000);

    // player
    int px = fx_to_int(s->posX);
    int py = fx_to_int(s->posY);
    gfx_fill_rect(px, py, s->playerW, s->playerH, 0xFF00FF00);

    gfx_draw_text_utf8(10, 10, 0xFFFFFFFF, "PHYSICS LAB (Fixed-Point)");
    gfx_draw_text_utf8(10, 28, 0xFFAAAAAA, "A/D move  |  W/SPACE jump (edge)");
}

static void physics_lab_on_mouse(app_t* app, int mx, int my,
                                 uint8_t pressed, uint8_t released, uint8_t buttons)
{
    (void)app; (void)mx; (void)my; (void)pressed; (void)released; (void)buttons;
}

static void physics_lab_on_key(app_t* app, uint16_t key) {
    physics_lab_t* s = st_of(app);
    if (!s) return;

    uint8_t sc = (uint8_t)(key & 0xFF);
    uint8_t idx = (uint8_t)(sc & 0x7F);

    // make/break
    if (sc & 0x80) s->key_down[idx] = 0;
    else           s->key_down[idx] = 1;

    // İPUCU:
    // jump edge için key_prev'i burada değil, draw sonunda güncelliyoruz.
}

static void physics_lab_on_destroy(app_t* app) {
    (void)app;
}

const app_vtbl_t physics_lab_vtbl = {
    .on_create  = physics_lab_on_create,
    .on_draw    = physics_lab_on_draw,
    .on_mouse   = physics_lab_on_mouse,
    .on_key     = physics_lab_on_key,
    .on_destroy = physics_lab_on_destroy
};