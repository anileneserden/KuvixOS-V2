#include <kernel/fs/vfs.h>
#include <kernel/fs/fat.h>
#include <kernel/fs/kvxfs.h>
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

void cmd_mkdir(int argc, char** argv) {
    int p_flag = 0;
    int arg_index = 1;

    if (argc >= 2 && strcmp(argv[1], "-p") == 0) {
        p_flag = 1;
        arg_index = 2;
    }

    if (argc <= arg_index) {
        commands_puts("Kullanim: mkdir [-p] <dizin>\n");
        return;
    }

    char path[256];
    build_path(path, sizeof(path), argv[arg_index]);

    /* FAT root mkdir */
    if (strncmp(path, "/fat/", 5) == 0) {
        const char* fat_sub = path + 5;

        /* şimdilik sadece FAT root altında mkdir */
        if (!fat_sub[0] || strchr(fat_sub, '/')) {
            commands_puts("Hata: Su anda FAT icin sadece root dizinde mkdir destekleniyor.\n");
            return;
        }

        /* zaten var mı? */
        {
            char fat_check[256];
            fat_check[0] = '/';
            fat_check[1] = 0;
            strncat(fat_check, fat_sub, sizeof(fat_check) - 1 - (int)strlen(fat_check));

            if (fat_path_exists(fat_check)) {
                if (p_flag) {
                    return; /* sessiz geç */
                }

                commands_puts("Hata: Dizin zaten var: ");
                commands_puts(path);
                commands_puts("\n");
                return;
            }
        }

        if (fat_create_root_dir(fat_sub)) {
            commands_puts("FAT dizini olusturuldu: ");
            commands_puts(path);
            commands_puts("\n");
        } else {
            commands_puts("Hata: FAT dizini olusturulamadi: ");
            commands_puts(path);
            commands_puts("\n");
        }
        return;
    }

    /* persist tarafi varsa koru */
    if (strncmp(path, "/persist", 8) == 0) {
        int r = kvxfs_mkdir(path);

        if (r == 0) {
            commands_puts("Dizin olusturuldu: ");
            commands_puts(path);
            commands_puts("\n");
        } else {
            if (p_flag) {
                return; /* şimdilik sessiz geç */
            }

            commands_puts("Hata: Dizin olusturulamadi: ");
            commands_puts(path);
            commands_puts("\n");
        }
        return;
    }

    /* normal VFS */
    int r = vfs_mkdir(path);
    if (r >= 0) {
        commands_puts("Dizin olusturuldu: ");
        commands_puts(path);
        commands_puts("\n");
    } else {
        if (p_flag) {
            return; /* şimdilik sessiz geç */
        }

        commands_puts("Hata: Dizin olusturulamadi: ");
        commands_puts(path);
        commands_puts("\n");
    }
}

REGISTER_COMMAND(mkdir, cmd_mkdir, "Dizin olusturur");