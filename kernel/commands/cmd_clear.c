#include <kernel/printk.h>
#include <lib/commands.h>

void cmd_clear(int argc, char** argv) {
    (void)argc; (void)argv;
    
    // Terminale "ekranı temizle" sinyalini gönder (\f)
    printk("\f");
}

REGISTER_COMMAND(clear, cmd_clear, "Ekrani temizler");