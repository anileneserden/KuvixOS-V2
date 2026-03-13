#include <lib/commands.h>
#include <kernel/printk.h>

// shell.c içindeki fonksiyonu dışarıdan çağırıyoruz
extern void shell_logout(void);

void cmd_logout(int argc, char** argv) {
    (void)argc; (void)argv;
    printk("Oturum kapatiliyor...\n");
    shell_logout();
}

REGISTER_COMMAND(logout, cmd_logout, "Mevcut oturumu kapatir ve giriş ekranina döner");