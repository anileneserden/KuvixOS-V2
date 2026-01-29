#include <kernel/fs.h>
#include <kernel/printk.h>
#include <lib/string.h>
#include <lib/commands.h>

void cmd_ls(int argc, char** argv) {
    (void)argc; (void)argv;
    if (!fs_root) return;

    fs_node_t *curr = fs_root->next;
    printk("Dizin listeleniyor: /\n");
    printk("Isim            Boyut       Tip\n----            -----       ---\n");

    if (!curr) {
        printk("(Dizin bos)\n");
        return;
    }

    while (curr) {
        printk("%s", curr->name);
        int spaces = 16 - strlen(curr->name);
        for(int s = 0; s < (spaces < 1 ? 1 : spaces); s++) printk(" ");
        printk("%d bytes    %s\n", curr->length, (curr->flags & FS_DIRECTORY) ? "<DIR>" : "<FILE>");
        curr = curr->next;
    }
}
REGISTER_COMMAND(ls, cmd_ls, "Dizin icerigini listeler");