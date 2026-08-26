#include <lib/commands.h>
#include <kernel/fs/vfs.h>
#include <kernel/drivers/video/fb_console.h>
#include <init/session.h>
#include <kernel/printk.h>

void cmd_whoami(int argc, char** argv) {
    user_session_t* session = session_get_current();
    if (session) {
        printk("Kullanici: %s (UID: %u)\n", session->username, session->uid);
    } else {
        printk("Bilinmeyen oturum\n");
    }
}

REGISTER_COMMAND(whoami, cmd_whoami, "Aktif oturumdaki kullaniciyi gosterir");