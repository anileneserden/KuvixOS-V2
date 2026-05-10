#include <ui/theme/theme.h>
#include <kernel/fs/vfs.h>
#include <kernel/printk.h>
#include <kernel/memory/kmalloc.h>
#include <lib/string.h>
#include <stdint.h>

static int g_bootstrap_done = 0;

#define THEME_PATH "/persist/theme.kth"

static const char* g_default_kth =
"[window]\n"
"window_bg=0x202020\n"
"window_border=0x505050\n"
"window_title_bg=0x303030\n"
"window_title_text=0xFFFFFF\n"
"window_border_px=2\n"
"window_title_h=28\n"
"window_title_align=left\n"
"window_title_pad_l=12\n"
"window_title_pad_r=12\n"
"\n"
"[window_buttons]\n"
"layout=left\n"
"style=classic\n"
"size=16\n"
"gap=6\n"
"margin=8\n"
"pad_left=6\n"
"pad_right=6\n"
"order=close,max,min\n";

void ui_theme_bootstrap_default(void)
{
    printk("[THEME] bootstrap called\n");

    if (g_bootstrap_done) return;
    g_bootstrap_done = 1;

    const ui_theme_t* builtin = ui_get_builtin_theme();

    // 1️⃣ Her zaman base olarak built-in'i kopyala
    static ui_theme_t parsed;
    if (builtin)
        memcpy(&parsed, builtin, sizeof(parsed));
    else
        memset(&parsed, 0, sizeof(parsed));

    // 2️⃣ Dosyayı oku
    const uint32_t cap = 8192;
    uint32_t sz = 0;

    char* buf = (char*)kmalloc(cap + 1);
    if (!buf) {
        ui_set_theme(&parsed);
        return;
    }

    int r = vfs_read_all(THEME_PATH, (uint8_t*)buf, cap, &sz);

    if (r >= 0 && sz > 0) {
        if (sz > cap) sz = cap;
        buf[sz] = 0;

        ui_theme_load_from_kth(buf, &parsed);
        ui_set_theme(&parsed);

        printk("[THEME] loaded: %s (%u bytes)\n", THEME_PATH, (unsigned)sz);
    } else {
        printk("[THEME] no %s — creating default\n", THEME_PATH);

        int wr = vfs_write_all(
            THEME_PATH,
            (const uint8_t*)g_default_kth,
            (uint32_t)strlen(g_default_kth)
        );

        if (wr >= 0) {
            printk("[THEME] default theme written: %s\n", THEME_PATH);
        } else {
            printk("[THEME] ERROR: failed to write %s\n", THEME_PATH);
        }

        ui_set_theme(&parsed);
    }

    kfree(buf);
}