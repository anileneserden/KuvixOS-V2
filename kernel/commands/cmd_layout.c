#include <kernel/printk.h>
#include <kernel/kbd.h>
#include <lib/commands.h>
#include <lib/string.h>

void cmd_layout(int argc, char** argv) {
    if (argc < 2) {
        printk("Kullanim: layout <trq|us>\n");
        return;
    }

    // kbd.c veya layout.c içinde tanımladığımız fonksiyonu çağırıyoruz
    kbd_set_layout(argv[1]);

    // Değişikliği doğrula
    const kbd_layout_t* current = kbd_get_current_layout();
    printk("Klavye düzeni '%s' olarak değiştirildi.\n", current->name);
}

// Otomatik kayıt makrosu
REGISTER_COMMAND(layout, cmd_layout, "Klavye dil düzenini değiştirir (trq/us)");