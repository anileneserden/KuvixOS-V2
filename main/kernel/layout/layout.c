#include <kernel/kbd.h>
#include <lib/string.h> // strcmp burada tanımlı

// Diğer dosyalardan layout objelerini alıyoruz
extern kbd_layout_t layout_us;
extern kbd_layout_t layout_trq;

static kbd_layout_t* current_layout = &layout_us;

void kbd_set_layout(const char* name) {
    // strcmp iki string aynıysa 0 döndürür
    if (strcmp(name, "trq") == 0) {
        current_layout = &layout_trq;
    } else {
        current_layout = &layout_us;
    }
}

const kbd_layout_t* kbd_get_current_layout(void) {
    return current_layout;
}