#include <kernel/fs/vfs.h>
#include <kernel/printk.h>
#include <lib/commands.h>

static int ls_callback(const char* name, uint32_t size, void* u) {
    (void)u;
    if (size == 0xFFFFFFFF) {
        printk("[DIR]  %s\n", name);
    } else {
        printk("%8u byte  %s\n", size, name);
    }
    return 0; 
}

void cmd_ls(int argc, char** argv) {
    const char* path = (argc > 1) ? argv[1] : "/";
    vfs_list(path, ls_callback, (void*)0);
}

REGISTER_COMMAND(ls, cmd_ls, "Dizin icerigini listeler");