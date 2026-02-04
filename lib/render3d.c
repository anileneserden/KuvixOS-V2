#include <lib/render3d.h>

vec2_t project_to_screen(vec3_t v3, int width, int height) {
    vec2_t v2;
    int focal_length = 400; // Kamera odağı
    int z_offset = 100;     // Küpü kameradan uzaklaştıran derinlik

    // Z ekseni 0 olamaz (Division by zero koruması)
    int z_depth = v3.z + z_offset;
    if (z_depth <= 0) z_depth = 1;

    // Perspektif Projeksiyon Formülü: (x * f) / z
    v2.x = (v3.x * focal_length) / z_depth + (width / 2);
    v2.y = (v3.y * focal_length) / z_depth + (height / 2);

    return v2;
}