#include <ui/apps/cube_app.h>

#include <kernel/drivers/video/gfx.h>
#include <kernel/time.h>

#include <lib/math.h>
#include <lib/string.h>

#include <ui/wm.h>

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int x;
    int y;
    int z;
} point3_t;

typedef struct {
    int x;
    int y;
} point2_t;

#define CUBE_FP 1024

static const point3_t k_cube_vertices[8] = {
    { -60, -60, -60 },
    {  60, -60, -60 },
    {  60,  60, -60 },
    { -60,  60, -60 },
    { -60, -60,  60 },
    {  60, -60,  60 },
    {  60,  60,  60 },
    { -60,  60,  60 },
};

static const uint8_t k_cube_edges[12][2] = {
    { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
    { 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
    { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },
};

static void draw_rect_1px(int x, int y, int w, int h, uint32_t color) {
    if (w <= 0 || h <= 0) return;
    gfx_fill_rect(x, y, w, 1, color);
    gfx_fill_rect(x, y + h - 1, w, 1, color);
    gfx_fill_rect(x, y, 1, h, color);
    gfx_fill_rect(x + w - 1, y, 1, h, color);
}

static void cube_button_rect(int client_w, int* x, int* y, int* w, int* h) {
    *w = 120;
    *h = 18;
    *x = client_w - *w - 10;
    *y = 5;
}

static bool cube_button_hit(int client_w, int mx, int my) {
    int x, y, w, h;
    cube_button_rect(client_w, &x, &y, &w, &h);
    return (mx >= x && my >= y && mx < x + w && my < y + h);
}

static void cube_mat_identity(int32_t out[9]) {
    for (int i = 0; i < 9; i++) out[i] = 0;
    out[0] = CUBE_FP;
    out[4] = CUBE_FP;
    out[8] = CUBE_FP;
}

static void cube_mat_copy(int32_t dst[9], const int32_t src[9]) {
    for (int i = 0; i < 9; i++) dst[i] = src[i];
}

static void cube_mat_mul(int32_t out[9], const int32_t a[9], const int32_t b[9]) {
    int32_t temp[9];

    for (int row = 0; row < 3; row++) {
        for (int col = 0; col < 3; col++) {
            int64_t sum = 0;
            for (int k = 0; k < 3; k++) {
                sum += (int64_t)a[row * 3 + k] * (int64_t)b[k * 3 + col];
            }
            temp[row * 3 + col] = (int32_t)(sum / CUBE_FP);
        }
    }

    cube_mat_copy(out, temp);
}

static uint32_t cube_isqrt_u64(uint64_t n) {
    uint64_t bit = 1ULL << 62;
    uint64_t res = 0;

    while (bit > n) bit >>= 2;

    while (bit != 0) {
        if (n >= res + bit) {
            n -= res + bit;
            res = (res >> 1) + bit;
        } else {
            res >>= 1;
        }
        bit >>= 2;
    }

    return (uint32_t)res;
}

static int64_t cube_dot3(const int32_t* a, const int32_t* b) {
    return (int64_t)a[0] * b[0] + (int64_t)a[1] * b[1] + (int64_t)a[2] * b[2];
}

static void cube_normalize3(int32_t* v) {
    uint64_t len2 = (uint64_t)cube_dot3(v, v);
    uint32_t len;

    if (len2 == 0) return;

    len = cube_isqrt_u64(len2);
    if (len == 0) return;

    v[0] = (int32_t)(((int64_t)v[0] * CUBE_FP) / len);
    v[1] = (int32_t)(((int64_t)v[1] * CUBE_FP) / len);
    v[2] = (int32_t)(((int64_t)v[2] * CUBE_FP) / len);
}

static void cube_cross3(int32_t* out, const int32_t* a, const int32_t* b) {
    out[0] = (int32_t)((((int64_t)a[1] * b[2]) - ((int64_t)a[2] * b[1])) / CUBE_FP);
    out[1] = (int32_t)((((int64_t)a[2] * b[0]) - ((int64_t)a[0] * b[2])) / CUBE_FP);
    out[2] = (int32_t)((((int64_t)a[0] * b[1]) - ((int64_t)a[1] * b[0])) / CUBE_FP);
}

static void cube_orthonormalize(int32_t mat[9]) {
    int32_t x[3] = { mat[0], mat[1], mat[2] };
    int32_t y[3] = { mat[3], mat[4], mat[5] };
    int32_t z[3];
    int64_t proj;

    cube_normalize3(x);

    proj = cube_dot3(y, x) / CUBE_FP;
    y[0] -= (int32_t)(((int64_t)x[0] * proj) / CUBE_FP);
    y[1] -= (int32_t)(((int64_t)x[1] * proj) / CUBE_FP);
    y[2] -= (int32_t)(((int64_t)x[2] * proj) / CUBE_FP);
    cube_normalize3(y);

    cube_cross3(z, x, y);
    cube_normalize3(z);
    cube_cross3(y, z, x);
    cube_normalize3(y);

    mat[0] = x[0]; mat[1] = x[1]; mat[2] = x[2];
    mat[3] = y[0]; mat[4] = y[1]; mat[5] = y[2];
    mat[6] = z[0]; mat[7] = z[1]; mat[8] = z[2];
}

static void cube_make_rot_x(int angle, int32_t out[9]) {
    int32_t sine = (math_sin(angle) * CUBE_FP) / 100;
    int32_t cosine = (math_cos(angle) * CUBE_FP) / 100;

    cube_mat_identity(out);
    out[4] = cosine;
    out[5] = -sine;
    out[7] = sine;
    out[8] = cosine;
}

static void cube_make_rot_y(int angle, int32_t out[9]) {
    int32_t sine = (math_sin(angle) * CUBE_FP) / 100;
    int32_t cosine = (math_cos(angle) * CUBE_FP) / 100;

    cube_mat_identity(out);
    out[0] = cosine;
    out[2] = sine;
    out[6] = -sine;
    out[8] = cosine;
}

static void cube_apply_drag_rotation(cube_app_t* state, int dx, int dy) {
    int32_t rot_x[9];
    int32_t rot_y[9];
    int32_t combined[9];
    int32_t updated[9];
    int yaw = -dx;
    int pitch = -dy;

    cube_make_rot_y(yaw, rot_y);
    cube_make_rot_x(pitch, rot_x);
    cube_mat_mul(combined, rot_x, rot_y);
    cube_mat_mul(updated, combined, state->orientation);
    cube_orthonormalize(updated);
    cube_mat_copy(state->orientation, updated);
}

static point3_t transform_point(point3_t point, const int32_t mat[9]) {
    point3_t out;
    int64_t x = (int64_t)mat[0] * point.x + (int64_t)mat[1] * point.y + (int64_t)mat[2] * point.z;
    int64_t y = (int64_t)mat[3] * point.x + (int64_t)mat[4] * point.y + (int64_t)mat[5] * point.z;
    int64_t z = (int64_t)mat[6] * point.x + (int64_t)mat[7] * point.y + (int64_t)mat[8] * point.z;

    out.x = (int)(x / CUBE_FP);
    out.y = (int)(y / CUBE_FP);
    out.z = (int)(z / CUBE_FP);
    return out;
}

static point2_t project_point(point3_t point, int width, int height) {
    point2_t projected;
    int viewer_distance = 260;
    int fov = 190;
    int depth = point.z + viewer_distance;

    if (depth < 32) depth = 32;

    projected.x = (point.x * fov) / depth + (width / 2);
    projected.y = (point.y * fov) / depth + (height / 2);

    return projected;
}

static void cube_on_create(app_t* app) {
    cube_app_t* state = (cube_app_t*)app->user;

    memset(state, 0, sizeof(*state));
    state->window_id = app->win_id;
    state->auto_rotate = 1;
    state->last_tick = g_ticks_ms;
    cube_mat_identity(state->orientation);
    cube_apply_drag_rotation(state, -32, -22);
    app->wants_continuous_redraw = 1;
    wm_invalidate_window(state->window_id);
}

static void cube_on_draw(app_t* app) {
    cube_app_t* state = (cube_app_t*)app->user;
    ui_rect_t client = wm_get_client_rect(state->window_id);
    point2_t points[8];
    int button_x, button_y, button_w, button_h;

    gfx_fill_rect(0, 0, client.w, client.h, 0xFF0B1020);
    gfx_fill_rect(0, 0, client.w, 28, 0xFF121A30);
    draw_rect_1px(0, 0, client.w, client.h, 0xFF22304A);

    cube_button_rect(client.w, &button_x, &button_y, &button_w, &button_h);
    gfx_fill_rect(button_x, button_y, button_w, button_h,
                  state->auto_rotate ? 0xFF1F8A5B : 0xFF6A2F39);
    draw_rect_1px(button_x, button_y, button_w, button_h, 0xFFD8E6FF);

    gfx_draw_text(10, 9, 0xFFD8E6FF, "3D Cube Demo");
    gfx_draw_text(button_x + 8, button_y + 5, 0xFFFFFFFF,
                  state->auto_rotate ? "Auto Rotate: ON" : "Auto Rotate: OFF");
    gfx_draw_text(10, client.h - 18, 0xFF9AA8C0,
                  state->auto_rotate ? "Mouse ile surukle, ustten auto rotate kapatilabilir"
                                     : "Mouse ile surukle, ustteki butondan auto rotate acilabilir");

    for (int i = 0; i < 8; i++) {
        point3_t rotated = transform_point(k_cube_vertices[i], state->orientation);
        points[i] = project_point(rotated, client.w, client.h);
    }

    for (int i = 0; i < 12; i++) {
        int a = k_cube_edges[i][0];
        int b = k_cube_edges[i][1];
        uint32_t color = (i < 4) ? 0xFF57D3FF : ((i < 8) ? 0xFFFFC857 : 0xFFFFFFFF);
        gfx_draw_line(points[a].x, points[a].y, points[b].x, points[b].y, color);
    }

    for (int i = 0; i < 8; i++) {
        gfx_fill_rect(points[i].x - 2, points[i].y - 2, 5, 5, 0xFFFFFFFF);
    }
}

static void cube_on_mouse(app_t* app, int mx, int my, uint8_t pressed, uint8_t released, uint8_t buttons) {
    cube_app_t* state = (cube_app_t*)app->user;
    ui_rect_t client = wm_get_client_rect(state->window_id);
    bool left_down = (buttons & 0x01u) != 0;

    (void)released;

    if ((pressed & 0x01u) && cube_button_hit(client.w, mx, my)) {
        state->auto_rotate = !state->auto_rotate;
        state->dragging = 0;
        app->wants_continuous_redraw = state->auto_rotate;
        wm_invalidate_window(state->window_id);
        return;
    }

    if (left_down) {
        if (state->dragging) {
            cube_apply_drag_rotation(state, mx - state->last_mx, my - state->last_my);
            wm_invalidate_window(state->window_id);
        }
        state->dragging = 1;
    } else {
        state->dragging = 0;
    }

    state->last_mx = mx;
    state->last_my = my;
}

static void cube_on_update(app_t* app) {
    cube_app_t* state = (cube_app_t*)app->user;
    uint32_t now = g_ticks_ms;
    uint32_t dt = now - state->last_tick;

    app->wants_continuous_redraw = state->auto_rotate;
    state->last_tick = now;

    if (dt > 100) dt = 16;
    if (state->dragging || !state->auto_rotate) return;

    state->spin_accum += (int32_t)(dt * 9);
    while (state->spin_accum >= 10) {
        cube_apply_drag_rotation(state, -1, 0);
        state->spin_accum -= 10;
    }
    wm_invalidate_window(state->window_id);
}

const app_vtbl_t cube_app_vtbl = {
    .on_create = cube_on_create,
    .on_mouse = cube_on_mouse,
    .on_update = cube_on_update,
    .on_draw = cube_on_draw,
};