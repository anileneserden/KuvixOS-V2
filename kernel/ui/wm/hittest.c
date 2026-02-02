#include <ui/wm/hittest.h>
#include <ui/window/window.h>

/**
 * Pencerenin hangi bölgesine tıklandığını tespit eder.
 */
wm_hittest_t ui_chrome_hittest(ui_window_t* win, int mx, int my) {
    if (!win) return HT_NONE;

    int border = 5; // Kenarları yakalamak için hassasiyet payı

    // 1. KÖŞELER (Önce köşeleri kontrol etmeliyiz)
    // Sol Üst
    if (mx >= win->x && mx <= win->x + border && my >= win->y && my <= win->y + border)
        return HT_RESIZE_TOP_LEFT;
    // Sağ Üst
    if (mx >= win->x + win->w - border && mx <= win->x + win->w && my >= win->y && my <= win->y + border)
        return HT_RESIZE_TOP_RIGHT;
    // Sol Alt
    if (mx >= win->x && mx <= win->x + border && my >= win->y + win->h - border && my <= win->y + win->h)
        return HT_RESIZE_BOTTOM_LEFT;
    // Sağ Alt (Senin orijinal alanın: 15px biraz daha geniş bırakılmış, kalsın)
    if (mx >= (win->x + win->w - 15) && mx <= (win->x + win->w) &&
        my >= (win->y + win->h - 15) && my <= (win->y + win->h)) {
        return HT_RESIZE_RIGHT_BOTTOM; 
    }

    // 2. KENARLAR
    if (mx >= win->x && mx <= win->x + border && my >= win->y && my <= win->y + win->h)
        return HT_RESIZE_LEFT;
    if (mx >= win->x + win->w - border && mx <= win->x + win->w && my >= win->y && my <= win->y + win->h)
        return HT_RESIZE_RIGHT;
    if (my >= win->y && my <= win->y + border && mx >= win->x && mx <= win->x + win->w)
        return HT_RESIZE_TOP;
    if (my >= win->y + win->h - border && my <= win->y + win->h && mx >= win->x && mx <= win->x + win->w)
        return HT_RESIZE_BOTTOM;

    // 3. BUTONLAR VE BAŞLIK (Senin orijinal kodun)
    if (mx >= (win->x + win->w - 24) && mx <= (win->x + win->w - 4) &&
        my >= (win->y + 4) && my <= (win->y + 20)) {
        return HT_BTN_CLOSE;
    }

    if (mx >= win->x && mx <= (win->x + win->w) &&
        my >= win->y && my <= (win->y + 24)) {
        return HT_TITLE;
    }

    // 4. İÇERİK
    if (mx >= win->x && mx <= (win->x + win->w) &&
        my >= win->y && my <= (win->y + win->h)) {
        return HT_CLIENT;
    }

    return HT_NONE;
}