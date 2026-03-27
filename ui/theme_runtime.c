#include <ui/theme.h>
#include <kernel/fs/vfs.h>
#include <kernel/memory/kmalloc.h>
#include <kernel/printk.h>
#include <lib/string.h>

#ifndef THEME_PATH
#define THEME_PATH "/persist/theme.kth"
#endif

static ui_theme_t g_active_theme;

// ------------------------------------------------------------
// Active theme getter/setter
// ------------------------------------------------------------
const ui_theme_t* ui_get_theme(void)
{
    return &g_active_theme;
}

void ui_set_theme(const ui_theme_t* th)
{
    if (th) g_active_theme = *th;
}

// ------------------------------------------------------------
// Reload from disk (/persist/theme.kth)
// ------------------------------------------------------------
void ui_theme_reload_from_disk(void)
{
    static ui_theme_t parsed;

    // 1) built-in theme'i baz al (eksik key'ler 0 olmasın)
    const ui_theme_t* base = ui_get_builtin_theme();
    if (base) memcpy(&parsed, base, sizeof(parsed));
    else      memset(&parsed, 0, sizeof(parsed));

    // 2) dosyayı oku
    const uint32_t cap = 8192;
    uint32_t sz = 0;

    char* buf = (char*)kmalloc(cap + 1);
    if (!buf) {
        printk("[THEME] reload: kmalloc failed\n");
        ui_set_theme(&parsed);
        return;
    }

    int r = vfs_read_all(THEME_PATH, (uint8_t*)buf, cap, &sz);
    if (r >= 0 && sz > 0) {
        if (sz > cap) sz = cap;
        buf[sz] = 0;

        // 3) base üstüne override
        ui_theme_load_from_kth(buf, &parsed);
        ui_set_theme(&parsed);

        printk("[THEME] reloaded: %s (%u bytes)\n", THEME_PATH, (unsigned)sz);
    } else {
        ui_set_theme(&parsed);
        printk("[THEME] reload: using built-in (no %s)\n", THEME_PATH);
    }

    kfree(buf);
}