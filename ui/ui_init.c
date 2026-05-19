// ui/ui_init.c
#include <ui/ui_init.h>
#include <kernel/printk.h>

static int g_ui_inited = 0;

void ui_init(void) {
    if (g_ui_inited) return;
    g_ui_inited = 1;

    // Pineapple GUI sistemleri uçtuğu için sadece çekirdek logu basıyoruz
    printk("[TTY] Terminal oturum yoneticisi ilklendirildi.\n");
}