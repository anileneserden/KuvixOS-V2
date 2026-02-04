#include <lib/math.h>

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