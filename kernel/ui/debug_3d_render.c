#include <kernel/drivers/input/input_manager.h>
#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/time.h>
#include <lib/math.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct { int x, y, z; } Point3D;
typedef struct { int x, y; } Point2D;

static Point3D cube_vertices[] = {
    {-50, -50, -50}, { 50, -50, -50},
    { 50,  50, -50}, {-50,  50, -50},
    {-50, -50,  50}, { 50, -50,  50},
    { 50,  50,  50}, {-50,  50,  50}
};

static int cube_faces[6][4] = {
    {0, 1, 2, 3},
    {1, 5, 6, 2},
    {5, 4, 7, 6},
    {4, 0, 3, 7},
    {3, 2, 6, 7},
    {4, 5, 1, 0}
};

static uint32_t face_colors[6] = {
    0xFF0000, 0x00FF00, 0x0000FF,
    0xFFFF00, 0xFF00FF, 0x00FFFF
};

static Point2D project(Point3D p) {
    Point2D out;

    int fov = 256;
    int viewer_dist = 220;

    int width  = (int)fb_get_width();
    int height = (int)fb_get_height();

    int denom = (p.z + viewer_dist);
    if (denom < 10) denom = 10;

    int aspect_mul = (width * 100) / (height ? height : 1);

    out.x = (((p.x * fov) * aspect_mul) / 100) / denom + (width / 2);
    out.y = ((p.y * fov) / denom) + (height / 2);
    return out;
}

static Point3D rotate(Point3D p, int ax, int ay) {
    Point3D res = p;
    int s, c;
    int tmp;

    s = math_sin(ax);
    c = math_cos(ax);
    tmp   = (res.y * c - res.z * s) / 100;
    res.z = (res.y * s + res.z * c) / 100;
    res.y = tmp;

    s = math_sin(ay);
    c = math_cos(ay);
    tmp   = (res.x * c + res.z * s) / 100;
    res.z = (-res.x * s + res.z * c) / 100;
    res.x = tmp;

    return res;
}

void debug_3d_render_loop(void) {
    static uint32_t last_ms = 0;

    static int angle_x = 25;
    static int angle_y = 45;

    static int32_t acc_x = 0;
    static int32_t acc_y = 0;

    const int32_t speed_x_deg_s = 60;
    const int32_t speed_y_deg_s = 90;

    const int mouse_sens = 1;

    // --- dt ---
    uint32_t now = g_ticks_ms;

    uint32_t dt_ms;
    if (last_ms == 0) {
        dt_ms = 16;
    } else {
        dt_ms = now - last_ms;
        if (dt_ms == 0) dt_ms = 16;
        if (dt_ms > 100) dt_ms = 100;
    }
    last_ms = now;

    // --- Mouse (frame delta + buttons) ---
    int mdx = 0, mdy = 0;
    input_mouse_frame_delta(&mdx, &mdy);
    uint8_t btn = input_mouse_buttons();

    // Eğer sol click mask farklıysa test için geçici: (btn != 0)
    bool dragging = (btn & 0x01) != 0;

    // İstersen dev delta’ları clamp et:
    if (mdx > 60) mdx = 60;
    if (mdx < -60) mdx = -60;
    if (mdy > 60) mdy = 60;
    if (mdy < -60) mdy = -60;

    if (dragging) {
        angle_y -= (mdx * mouse_sens);
        angle_x -= (mdy * mouse_sens);

        // mouse bırakınca auto yumuşak dönsün
        acc_x = 0;
        acc_y = 0;
    } else {
        acc_x += speed_x_deg_s * (int32_t)dt_ms;
        acc_y += speed_y_deg_s * (int32_t)dt_ms;

        while (acc_x >= 1000) { angle_x++; acc_x -= 1000; }
        while (acc_y >= 1000) { angle_y++; acc_y -= 1000; }
    }

    angle_x %= 360; if (angle_x < 0) angle_x += 360;
    angle_y %= 360; if (angle_y < 0) angle_y += 360;

    // --- render ---
    gfx_clear(0x050510);

    Point2D projected[8];
    for (int i = 0; i < 8; i++) {
        Point3D r = rotate(cube_vertices[i], angle_x, angle_y);
        projected[i] = project(r);
    }

    for (int i = 0; i < 6; i++) {
        int x[4], y[4];
        for (int j = 0; j < 4; j++) {
            int idx = cube_faces[i][j];
            x[j] = projected[idx].x;
            y[j] = projected[idx].y;
        }

        int v1x = x[1] - x[0];
        int v1y = y[1] - y[0];
        int v2x = x[2] - x[0];
        int v2y = y[2] - y[0];

        if ((v1x * v2y - v1y * v2x) > 0) {
            gfx_fill_polygon(x, y, 4, face_colors[i]);
            for (int j = 0; j < 4; j++) {
                int n = (j + 1) & 3;
                gfx_draw_line(x[j], y[j], x[n], y[n], 0x000000);
            }
        }
    }

    fb_present();
}
