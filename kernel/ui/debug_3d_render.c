#include <kernel/drivers/video/gfx.h>
#include <kernel/drivers/video/fb.h>
#include <kernel/drivers/input/keyboard.h>
#include <lib/math.h>

// 1. Köşeler (8 adet)
static int vertices[8][3] = {
    {-30, -30, -30}, {30, -30, -30}, {30, 30, -30}, {-30, 30, -30},
    {-30, -30,  30}, {30, -30,  30}, {30, 30,  30}, {-30, 30,  30}
};

// 2. Yüzeyler (6 adet kare yüzey)
static int faces[6][4] = {
    {0, 1, 2, 3}, // Arka
    {4, 5, 6, 7}, // Ön
    {0, 4, 7, 3}, // Sol
    {1, 5, 6, 2}, // Sağ
    {3, 2, 6, 7}, // Üst
    {0, 1, 5, 4}  // Alt
};

static uint32_t face_colors[6] = {
    0xFF0000, // Kırmızı
    0x00FF00, // Yeşil
    0x0000FF, // Mavi
    0xFFFF00, // Sarı
    0xFF00FF, // Magenta
    0x00FFFF  // Cyan
};

static int angle_x = 0;
static int angle_y = 0;
static int zoom = 150;

void debug_3d_render_loop(int screen_width, int screen_height) {
    // Sıralama yapısı
    typedef struct {
        int index;
        int z;
    } face_z_t;

    face_z_t sorted_faces[6];
    int proj_x[8], proj_y[8], rot_z[8];

    // 1. INPUT (WASD + QE)
    while (kbd_has_character()) {
        uint16_t event = kbd_pop_event();
        uint8_t scancode = event & 0x7F;
        int pressed = !(event & 0x80);

        if (pressed) {
            if (scancode == 0x11) angle_x -= 10; // W
            if (scancode == 0x1F) angle_x += 10; // S
            if (scancode == 0x1E) angle_y -= 10; // A
            if (scancode == 0x20) angle_y += 10; // D
            if (scancode == 0x10) zoom -= 10;    // Q
            if (scancode == 0x12) zoom += 10;    // E
        }
    }

    // --- OTOMATİK DÖNDÜRME ---
    // Her karede açıları biraz artıralım (Hızı buradan ayarlayabilirsin)
    angle_x = (angle_x + 1) % 360; 
    angle_y = (angle_y + 2) % 360;

    // --- MATEMATİKSEL HAZIRLIK ---
    int sx = math_sin(angle_x), cx = math_cos(angle_x);
    int sy = math_sin(angle_y), cy = math_cos(angle_y);

    // 3. KOORDİNAT HESAPLAMA (X ve Y Rotasyonu + Projeksiyon)
    for (int i = 0; i < 8; i++) {
        int x = vertices[i][0];
        int y = vertices[i][1];
        int z = vertices[i][2];

        // X Rotasyonu
        int y1 = (y * cx - z * sx) / 100;
        int z1 = (y * sx + z * cx) / 100;

        // Y Rotasyonu
        int x2 = (x * cy + z1 * sy) / 100;
        int z2 = (-x * sy + z1 * cy) / 100;

        rot_z[i] = z2; // Sıralama için derinlik bilgisini tut

        // Projeksiyon
        int z_depth = z2 + zoom;
        if (z_depth <= 0) z_depth = 1;
        proj_x[i] = (x2 * 300) / z_depth + (screen_width / 2);
        proj_y[i] = (y1 * 300) / z_depth + (screen_height / 2);
    }

    // 4. Z-SORTING (Painter's Algorithm)
    for (int i = 0; i < 6; i++) {
        sorted_faces[i].index = i;
        sorted_faces[i].z = (rot_z[faces[i][0]] + rot_z[faces[i][1]] + 
                             rot_z[faces[i][2]] + rot_z[faces[i][3]]) / 4;
    }

    for (int i = 0; i < 5; i++) {
        for (int j = i + 1; j < 6; j++) {
            if (sorted_faces[i].z < sorted_faces[j].z) {
                face_z_t temp = sorted_faces[i];
                sorted_faces[i] = sorted_faces[j];
                sorted_faces[j] = temp;
            }
        }
    }

    // 5. ÇİZİM
    fb_clear(0x000000);

    for (int i = 0; i < 6; i++) {
        int f = sorted_faces[i].index;
        int x_coords[4], y_coords[4];

        for (int v = 0; v < 4; v++) {
            x_coords[v] = proj_x[faces[f][v]];
            y_coords[v] = proj_y[faces[f][v]];
        }
        
        gfx_fill_polygon(x_coords, y_coords, 4, face_colors[f]);
    }

    fb_present();
}