#include <ui/ui_settings.h>

static bool g_show_extensions = false;

bool ui_get_show_extensions(void) {
    return g_show_extensions;
}

void ui_set_show_extensions(bool v) {
    g_show_extensions = v;
}

void ui_toggle_show_extensions(void) {
    g_show_extensions = !g_show_extensions;
}