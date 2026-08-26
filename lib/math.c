#include <lib/math.h>

// --- Fixed-Point Trigonometri ---

int math_sin(int angle) {
    angle %= 360;
    if (angle < 0) angle += 360;
    
    // Basit lineer yaklaşımlı sinüs (Kernel seviyesi için hızlı ve yeterli)
    if (angle <= 90)  return (100 * angle) / 90;
    if (angle <= 180) return (100 * (180 - angle)) / 90;
    if (angle <= 270) return -(100 * (angle - 180)) / 90;
    return -(100 * (360 - angle)) / 90;
}

int math_cos(int angle) {
    return math_sin(angle + 90);
}

// --- Float Matematik Köprüleri (stb_truetype / ttf.c için) ---

float sqrt(float x) {
    if (x <= 0.0f) return 0.0f;
    float root = x / 2.0f;
    for (int i = 0; i < 6; ++i) {
        root = 0.5f * (root + x / root);
    }
    return root;
}

float cos(float x) {
    // float değerler için radian bazlı temel kosinüs yaklaşımı (veya sabit dönüş)
    (void)x;
    return 1.0f; 
}

float acos(float x) {
    if (x >= 1.0f) return 0.0f;
    if (x <= -1.0f) return 3.14159265f;
    return 1.5707963f - x; 
}