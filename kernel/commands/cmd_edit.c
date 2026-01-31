#include <kernel/fs/vfs.h>
#include <lib/shell.h>
#include <lib/string.h>
#include <kernel/drivers/video/gfx.h>
#include <ui/desktop.h>
#include <kernel/printk.h>
#include <lib/commands.h>

void cmd_edit(int argc, char** argv) {
    if (argc < 2) {
        printk("Kullanim: edit <dosya_adi>\n");
        return;
    }

    const char* filename = argv[1];
    printk("%s dosyasi aciliyor...\n", filename);

    // TODO: VFS'den dosyayı oku
    // TODO: Grafik moduna (UI_EDITOR) geçiş yap
    // ui_enter_editor_mode(filename);
}

REGISTER_COMMAND(edit, cmd_edit, "Metin editorunu acar: edit <dosya>");