#include <kernel/fs/vfs.h>
#include <kernel/fs/fat.h>
#include <lib/commands.h>
#include <lib/string.h>

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

static int is_redirect_token(const char* s) {
    if (!s) return 0;
    return (strcmp(s, ">") == 0 || strcmp(s, ">>") == 0);
}

void cmd_echo(int argc, char** argv) {
    if (argc < 2) {
        commands_puts("\n");
        return;
    }

    int redir_index = -1;
    int append_mode = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], ">") == 0) {
            redir_index = i;
            append_mode = 0;
            break;
        }
        if (strcmp(argv[i], ">>") == 0) {
            redir_index = i;
            append_mode = 1;
            break;
        }
    }

    /* redirect yoksa normal echo */
    if (redir_index < 0) {
        for (int i = 1; i < argc; i++) {
            commands_puts(argv[i]);
            if (i < argc - 1) commands_puts(" ");
        }
        commands_puts("\n");
        return;
    }

    /* operator var ama hedef yok */
    if (redir_index + 1 >= argc) {
        commands_puts("Hata: Redirect hedefi eksik.\n");
        return;
    }

    /* metni birleştir */
    char text[8192];
    text[0] = 0;

    for (int i = 1; i < redir_index; i++) {
        if (is_redirect_token(argv[i])) break;

        if ((int)strlen(text) + (int)strlen(argv[i]) + 2 >= (int)sizeof(text)) {
            commands_puts("Hata: echo metni cok uzun.\n");
            return;
        }

        strncat(text, argv[i], sizeof(text) - 1 - (int)strlen(text));
        if (i < redir_index - 1) {
            strncat(text, " ", sizeof(text) - 1 - (int)strlen(text));
        }
    }

    /* echo varsayılan olarak newline ekler */
    if ((int)strlen(text) + 1 < (int)sizeof(text) - 1) {
        strncat(text, "\n", sizeof(text) - 1 - (int)strlen(text));
    }

    char path[256];
    build_path(path, sizeof(path), argv[redir_index + 1]);

    /* FAT pseudo-mount */
    if (strncmp(path, "/fat/", 5) == 0) {
        const char* fat_sub = path + 5;
        int ok = 0;

        if (append_mode) {
            ok = fat_append_file_path(fat_sub, (const uint8_t*)text, (uint32_t)strlen(text));
        } else {
            ok = fat_write_file_path(fat_sub, (const uint8_t*)text, (uint32_t)strlen(text));
        }

        if (!ok) {
            commands_puts("Hata: FAT dosyasina yazilamadi: ");
            commands_puts(path);
            commands_puts("\n");
        }
        return;
    }

    /* normal VFS: sadece overwrite */
    if (append_mode) {
        commands_puts("Hata: Normal VFS icin >> henuz desteklenmiyor.\n");
        return;
    }

    {
        int r = vfs_write_all(path, (const uint8_t*)text, (uint32_t)strlen(text));
        if (r < 0) {
            commands_puts("Hata: Dosyaya yazilamadi: ");
            commands_puts(path);
            commands_puts("\n");
        }
    }
}

REGISTER_COMMAND(echo, cmd_echo, "Metni ekrana yazar veya dosyaya yonlendirir");