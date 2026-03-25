#include <ui/screen.h>
#include <kernel/drivers/video/fb.h>

void ui_screen_render(const ui_screen_t* screen) {
    if (!screen) return;

    fb_clear(screen->background_color);
}