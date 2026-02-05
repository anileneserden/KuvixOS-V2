#include <ui/messagebox.h>
#include <kernel/drivers/video/gfx.h>
#include <lib/string.h>

// Private (Statik) Değişkenler
static char _title[64];
static char _text[256];
static bool _visible = false;
static MB_BTNS_T _active_btns;

// Pencere Boyut ve Konum (1024x768'e göre ortalanmış)
static int _win_w = 320, _win_h = 150;
static int _win_x, _win_y;

// --- Fonksiyon Gövdeleri ---

static void _show(const char* title, const char* text, MB_ICON_T icon, MB_BTNS_T buttons) {
    strncpy(_title, title, 63);
    strncpy(_text, text, 255);
    _active_btns = buttons;
    _visible = true;

    // Koordinatları hesapla
    _win_x = (1024 - _win_w) / 2;
    _win_y = (768 - _win_h) / 2;
}

static void _close(void) {
    _visible = false;
}

// --- Sistem Seviyesi Fonksiyonlar ---

void messagebox_draw(void) {
    if (!_visible) return;

    // 1. Pencere Gövdesi ve Gölge
    gfx_fill_rect(_win_x + 4, _win_y + 4, _win_w, _win_h, 0x222222); // Gölge
    gfx_fill_rect(_win_x, _win_y, _win_w, _win_h, 0xDDDDDD);         // Arka Plan
    gfx_draw_rect(_win_x, _win_y, _win_w, _win_h, 0x000000);         // Kenarlık

    // 2. Başlık Çubuğu
    gfx_fill_rect(_win_x, _win_y, _win_w, 25, 0x0000AA);
    gfx_draw_text(_win_x + 10, _win_y + 5, 0xFFFFFF, _title);

    // 3. Mesaj İçeriği
    gfx_draw_text(_win_x + 20, _win_y + 50, 0x000000, _text);

    // 4. Dinamik Butonlar
    if (_active_btns == MB_BTNS_OK) {
        int bx = _win_x + (_win_w/2) - 40;
        int by = _win_y + _win_h - 40;
        gfx_fill_rect(bx, by, 80, 25, 0xAAAAAA);
        gfx_draw_rect(bx, by, 80, 25, 0x000000);
        gfx_draw_text(bx + 15, by + 5, 0x000000, "Tamam");
    } 
    else if (_active_btns == MB_BTNS_YESNO) {
        // Evet Butonu
        int ex = _win_x + 40;
        int ey = _win_y + _win_h - 40;
        gfx_fill_rect(ex, ey, 80, 25, 0xAAAAAA);
        gfx_draw_rect(ex, ey, 80, 25, 0x000000);
        gfx_draw_text(ex + 20, ey + 5, 0x000000, "Evet");

        // Hayır Butonu
        int hx = _win_x + _win_w - 120;
        int hy = _win_y + _win_h - 40;
        gfx_fill_rect(hx, hy, 80, 25, 0xAAAAAA);
        gfx_draw_rect(hx, hy, 80, 25, 0x000000);
        gfx_draw_text(hx + 15, hy + 5, 0x000000, "Hayir");
    }
}

void messagebox_handle_mouse(int mx, int my, bool pressed) {
    if (!_visible || !pressed) return;

    // Butonların konumunu çizimdekiyle birebir aynı hesaplamalıyız
    if (_active_btns == MB_BTNS_OK) {
        int bx = _win_x + (_win_w / 2) - 40;
        int by = _win_y + _win_h - 40;
        int bw = 80;
        int bh = 25;

        // Fare butonun sınırları içinde mi?
        if (mx >= bx && mx <= bx + bw && my >= by && my <= by + bh) {
            _visible = false; // Kapat!
        }
    } 
    else if (_active_btns == MB_BTNS_YESNO) {
        // Evet Butonu (Çizimdeki koordinatlarla aynı: _win_x + 40)
        if (mx >= _win_x + 40 && mx <= _win_x + 120 && 
            my >= _win_y + _win_h - 40 && my <= _win_y + _win_h - 15) {
            _visible = false;
        }
        // Hayır Butonu (Çizimdeki koordinatlarla aynı: _win_x + _win_w - 120)
        if (mx >= _win_x + _win_w - 120 && mx <= _win_x + _win_w - 40 && 
            my >= _win_y + _win_h - 40 && my <= _win_y + _win_h - 15) {
            _visible = false;
        }
    }
}

// --- Global Nesne Tanımları (En Altta Olmalı) ---

MessageBoxButtons_Wrapper MessageBoxButtons = {
    .OK = MB_BTNS_OK,
    .YesNo = MB_BTNS_YESNO
};

MessageBox_Namespace MessageBox = {
    .Show = _show,
    .Close = _close
};