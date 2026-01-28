#include <kernel/kbd.h>
#include <lib/string.h> // k_streq için

// Diğer dosyalardaki layout objelerini extern ile alıyoruz
extern kbd_layout_t layout_us;
extern kbd_layout_t layout_trq;

static kbd_layout_t* current_layout = &layout_us; // Varsayılan US

void kbd_set_layout(const char* name) {
    if (k_streq(name, "trq")) {
        current_layout = &layout_trq;
    } else {
        current_layout = &layout_us;
    }
}

const kbd_layout_t* kbd_get_current_layout(void) {
    return current_layout;
}