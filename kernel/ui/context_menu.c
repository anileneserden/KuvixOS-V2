#include <ui/context_menu.h>
#include <kernel/drivers/video/gfx.h>
#include <lib/string.h>

#define MAX_MENU_ITEMS 8

typedef struct {
    char text[32];
    void (*callback)(void);
} menu_item_t;

static menu_item_t menu_items[MAX_MENU_ITEMS];
static int menu_item_count = 0;
static int menu_x = 0, menu_y = 0;
static bool menu_visible = false;

// Listeyi temizle (Hata aldığın fonksiyon 1)
void context_menu_reset(void) {
    menu_item_count = 0;
}

// Yeni seçenek ekle (Hata aldığın fonksiyon 2)
void context_menu_add_item(const char* text, void (*callback)(void)) {
    if (menu_item_count < MAX_MENU_ITEMS) {
        strncpy(menu_items[menu_item_count].text, text, 31);
        menu_items[menu_item_count].callback = callback;
        menu_item_count++;
    }
}

void context_menu_show(int x, int y) {
    menu_x = x;
    menu_y = y;
    menu_visible = true;
}

void context_menu_hide(void) {
    menu_visible = false;
}

bool context_menu_is_visible(void) {
    return menu_visible;
}

// Menü öğesine tıklandığında ilgili fonksiyonu çalıştırır
void context_menu_handle_mouse(int mx, int my, bool clicked) {
    if (!menu_visible) return;

    int w = 150;
    int h = menu_item_count * 20;

    if (mx >= menu_x && mx <= menu_x + w && my >= menu_y && my <= menu_y + h) {
        if (clicked) {
            int index = (my - menu_y) / 20;
            if (index >= 0 && index < menu_item_count) {
                if (menu_items[index].callback) {
                    menu_items[index].callback();
                }
            }
            context_menu_hide();
        }
    } else if (clicked) {
        context_menu_hide();
    }
}

void context_menu_draw(void) {
    if (!menu_visible) return;

    int w = 150;
    int h = menu_item_count * 20;

    // Arka plan ve kenarlık
    gfx_fill_rect(menu_x, menu_y, w, h, 0xCCCCCC);
    gfx_draw_rect(menu_x, menu_y, w, h, 0x000000);

    for (int i = 0; i < menu_item_count; i++) {
        gfx_draw_text(menu_x + 5, menu_y + (i * 20) + 5, 0x000000, menu_items[i].text);
    }
}