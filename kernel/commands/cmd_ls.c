#include <kernel/fs/vfs.h>
#include <lib/commands.h>
#include <lib/string.h>
#include <kernel/drivers/video/fb_console.h>
#include <kernel/fs/kvxfs.h>
#include <kernel/printk.h>

void cmd_ls(int argc, char** argv) {
    const char* target_path = NULL;

    if (argc > 1) {
        target_path = argv[1];
    } else {
        target_path = vfs_get_cwd();
        
        if (!target_path) {
            target_path = "/";
        }
    }
    
    kvxfs_list_all(target_path);
}

REGISTER_COMMAND(ls, cmd_ls, "Dizin icerigini listeler");