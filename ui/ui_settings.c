#include <ui/ui_settings.h>

static bool     g_show_ext = true;
static uint32_t g_desktop_bg = 0xFF202020; // default

bool ui_get_show_extensions(void) { return g_show_ext; }
void ui_toggle_show_extensions(void) { g_show_ext = !g_show_ext; }

uint32_t ui_get_desktop_bg(void) { return g_desktop_bg; }
void ui_set_desktop_bg(uint32_t argb) { g_desktop_bg = argb; }