#include <lib/commands.h>
#include <kernel/printk.h>
#include <init/session.h>

extern int session_has_previous(void);
extern void session_restore_previous(void);

void cmd_exit(int argc, char** argv) {
    if (session_has_previous()) {
        session_restore_previous();
        return;
    }
    printk("Cikilacak aktif bir alt oturum yok.\n");
}

REGISTER_COMMAND(exit, cmd_exit, "Exits the current root sub-session and returns to previous user");