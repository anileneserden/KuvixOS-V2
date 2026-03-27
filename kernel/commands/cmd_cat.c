#include <kernel/fs/vfs.h>
#include <lib/string.h>
#include <lib/commands.h>

static void build_path(char* out, int out_sz, const char* arg) {
    out[0] = 0;
    if (!arg || !arg[0]) return;

    // absolute ise direkt
    if (arg[0] == '/') {
        strncpy(out, arg, out_sz - 1);
        out[out_sz - 1] = 0;
        return;
    }

    // relative: cwd + "/" + arg
    const char* cwd = commands_get_cwd();
    strncpy(out, cwd, out_sz - 1);
    out[out_sz - 1] = 0;

    int l = (int)strlen(out);
    if (l > 0 && out[l - 1] != '/') strncat(out, "/", out_sz - 1 - (int)strlen(out));
    strncat(out, arg, out_sz - 1 - (int)strlen(out));
}

void cmd_cat(int argc, char** argv) {
    if (argc < 2) {
        commands_puts("Kullanim: cat <dosya>\n");
        return;
    }

    char path[256];
    build_path(path, sizeof(path), argv[1]);

    uint8_t buffer[4096];
    uint32_t size = 0;

    int r = vfs_read_all(path, buffer, sizeof(buffer) - 1, &size);
    if (r >= 0) {
        commands_printf(buffer, size);
        commands_puts("\n");
    } else {
        commands_puts("Hata: Dosya okunamadi: ");
        commands_puts(path);
        commands_puts("\n");
    }
}

REGISTER_COMMAND(cat, cmd_cat, "Dosya icerigini okur ve ekrana basar");