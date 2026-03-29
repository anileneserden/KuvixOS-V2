#include <lib/commands.h>

// ls fonksiyonunun prototipi (veya commands.h içindeyse dahil et)
extern void cmd_ls(int argc, char** argv);

void cmd_dir(int argc, char** argv) {
    // dir çağrıldığında sadece ls'in fonksiyonunu tetikle
    cmd_ls(argc, argv);
}

REGISTER_COMMAND(dir, cmd_dir, "ls komutunun alternatifi (Dizin listeler)");