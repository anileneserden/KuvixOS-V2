#include <lib/commands.h>
#include <lib/string.h>
#include <kernel/power.h>

void cmd_shutdown(int argc, char** argv)
{
    // Argüman kontrolü (`shutdown --now` veya doğrudan `shutdown` aynı şeyi yapacak)
    if (argc >= 2) {
        if (strcmp(argv[1], "--now") && strcmp(argv[1], "now")) {
            commands_puts("Kullanim: shutdown [--now|now]\n");
            return;
        }
    }

    // 🚫 Grafiksel geri sayım ekranı kaldırıldı, doğrudan sistem kapatılıyor
    commands_puts("Sistem kapatiliyor...\n");
    power_shutdown();
}

REGISTER_COMMAND(shutdown, cmd_shutdown, "Sistemi guvenli bir sekilde kapatir");