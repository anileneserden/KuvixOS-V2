#pragma once
#include <stdint.h>
#include <stdbool.h>

void debug_overlay_set_enabled(bool en);
bool debug_overlay_is_enabled(void);

// Desktop tick sonunda çağır
void debug_overlay_draw(int mouse_x, int mouse_y, int last_dx, int last_dy,
                        int last_wheel_step, int wheel_total, uint8_t buttons);