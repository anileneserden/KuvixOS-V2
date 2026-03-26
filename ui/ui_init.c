#include <ui/ui_init.h>
#include <kernel/printk.h>

#include <ui/icons.h>
#include <ui/theme.h>
#include <ui/kui/cpp/test_ui_c_bridge.h>

static int g_ui_inited = 0;

void ui_init(void) {
    if (g_ui_inited) return;
    g_ui_inited = 1;

    printk("[UI] ui_init\n");

    // 1) ikonlar
    //ui_icons_init();

    // 2) tema
    //ui_theme_bootstrap_default();

    // 3) C++ KUI test
    printk("[UI] running KUI C++ test\n");
    kui_cpp_test_ui_run();

    // 4) sonra istersen wm / desktop başlatırsın
    // wm_init();
}