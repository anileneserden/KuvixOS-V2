#include <ui/theme.h>
#include <kernel/drivers/video/fb.h>

// built-in default theme (boot'ta bunu aktif edeceğiz)
static ui_theme_t g_builtin_theme;

static int g_theme_inited = 0;

static void init_builtin_once(void)
{
    if (g_theme_inited) return;
    g_theme_inited = 1;

    // burada senin eski built-in değerlerin neyse aynen onları doldur
    // örnek:
    g_builtin_theme.desktop_bg        = fb_rgb(0, 24, 40);

    g_builtin_theme.window_bg         = fb_rgb(230,230,230);
    g_builtin_theme.window_border     = fb_rgb(120,120,120);
    g_builtin_theme.window_title_bg   = fb_rgb(40,48,80);
    g_builtin_theme.window_title_text = fb_rgb(255,255,255);
    g_builtin_theme.window_corner_radius = 10;
    g_builtin_theme.window_border_px  = 2;
    g_builtin_theme.window_title_h    = 24;

    g_builtin_theme.textbox_bg           = fb_rgb(239,239,239);
    g_builtin_theme.textbox_border       = fb_rgb(170,170,170);
    g_builtin_theme.textbox_focus_border = fb_rgb(80,128,255);
    g_builtin_theme.textbox_text         = fb_rgb(0,0,0);
    g_builtin_theme.textbox_placeholder  = fb_rgb(136,136,136);
    g_builtin_theme.textbox_caret        = fb_rgb(0,0,0);

    g_builtin_theme.button_bg             = fb_rgb(200,200,200);
    g_builtin_theme.button_border         = fb_rgb(100,100,100);
    g_builtin_theme.button_hover_bg       = fb_rgb(220,220,220);
    g_builtin_theme.button_hover_border   = fb_rgb(80,80,80);
    g_builtin_theme.button_pressed_bg     = fb_rgb(180,180,180);
    g_builtin_theme.button_pressed_border = fb_rgb(60,60,60);
    g_builtin_theme.button_text           = fb_rgb(0,0,0);

    // dock vs varsa buraya koy
}

const ui_theme_t* ui_get_builtin_theme(void)
{
    init_builtin_once();
    return &g_builtin_theme;
}
