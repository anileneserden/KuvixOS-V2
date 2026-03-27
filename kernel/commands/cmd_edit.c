#include <kernel/fs/vfs.h>
#include <lib/string.h>
#include <lib/commands.h>

#include <ui/editor.h>
#include <ui/session.h>

static void build_path(char* out, int out_sz, const char* arg) {
    out[0] = 0;
    if (!arg || !arg[0]) return;

    if (arg[0] == '/') {
        strncpy(out, arg, out_sz - 1);
        out[out_sz - 1] = 0;
        return;
    }

    const char* cwd = commands_get_cwd();
    strncpy(out, cwd, out_sz - 1);
    out[out_sz - 1] = 0;

    int l = (int)strlen(out);
    if (l > 0 && out[l - 1] != '/') {
        strncat(out, "/", out_sz - 1 - (int)strlen(out));
    }
    strncat(out, arg, out_sz - 1 - (int)strlen(out));
}

void cmd_edit(int argc, char** argv) {
    if (argc < 2) {
        commands_puts("Kullanim: edit <dosya>\n");
        return;
    }

    char path[256];
    build_path(path, sizeof(path), argv[1]);

    vfs_stat_t st;
    int exists = (vfs_stat(path, &st) == 0);

    if (!exists) {
        vfs_write_all(path, (const uint8_t*)"", 0);
    }

    ui_session_switch(UI_SESSION_EDITOR);
    editor_open(path);
}

REGISTER_COMMAND(edit, cmd_edit, "Dosya duzenleyici (nano benzeri)");