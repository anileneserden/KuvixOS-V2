#include <kernel/vga.h>
#include <lib/commands.h>

void cmd_clear(int argc, char** argv) {
    (void)argc; (void)argv; // Parametreleri kullanmadığımız için uyarıyı engelliyoruz
    
    vga_clear();
}

REGISTER_COMMAND(clear, cmd_clear, "Ekranı temizler");