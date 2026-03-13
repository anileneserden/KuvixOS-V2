#include <lib/commands.h>
#include <kernel/user.h>
#include <kernel/printk.h>

void cmd_whoami(int argc, char** argv) {
    (void)argc; (void)argv; // Parametreleri kullanmıyoruz
    
    const char* current_user = user_get_current_name();
    printk("%s\n", current_user);
}

REGISTER_COMMAND(whoami, cmd_whoami, "Aktif kullanıcı adını gösterir");