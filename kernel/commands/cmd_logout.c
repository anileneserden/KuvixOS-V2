#include <lib/commands.h>
#include <kernel/printk.h>
#include <init/session.h>
#include <lib/shell.h>

extern void session_logout(void);

void cmd_logout(int argc, char** argv) {
    // Önce log mesajı
    printk("Oturum kapatiliyor...\n");
    
    // Oturumu tamamen sıfırla ve login ekranına dön
    session_logout();
}

REGISTER_COMMAND(logout, cmd_logout, "Logs out completely and returns to the login screen");