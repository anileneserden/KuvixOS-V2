#include <kernel/fs/fat.h>
#include <lib/commands.h>
#include <lib/string.h>
#include <lib/shell.h>

static void build_path(char* out, int out_sz, const char* arg) {
    out[0] = 0;
    if (!arg || !arg[0]) return;

    /* absolute */
    if (arg[0] == '/') {
        strncpy(out, arg, out_sz - 1);
        out[out_sz - 1] = 0;
        return;
    }

    /* relative */
    const char* cwd = commands_get_cwd();
    strncpy(out, cwd, out_sz - 1);
    out[out_sz - 1] = 0;

    int l = (int)strlen(out);
    if (l > 0 && out[l - 1] != '/') {
        strncat(out, "/", out_sz - 1 - (int)strlen(out));
    }
    strncat(out, arg, out_sz - 1 - (int)strlen(out));
}

/*
 * Genel normalize ama /fat pseudo-root altındayken
 * ".." ile /fat seviyesinin üstüne çıkmaya izin vermez.
 */
static void normalize_path(char* path) {
    char tmp[256];
    char result[256];
    char* parts[32];
    int part_count = 0;
    int fat_floor = 0; /* /fat için minimum part_count */

    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = 0;

    char* p = tmp;
    while (*p == '/') p++;

    while (*p) {
        char* start = p;
        while (*p && *p != '/') p++;

        char saved = *p;
        *p = 0;

        if (strcmp(start, ".") == 0 || strcmp(start, "") == 0) {
            /* skip */
        } else if (strcmp(start, "..") == 0) {
            if (part_count > fat_floor) {
                part_count--;
            }
        } else {
            parts[part_count++] = start;

            /* /fat pseudo-root'u koru */
            if (part_count == 1 && strcmp(start, "fat") == 0) {
                fat_floor = 1;
            }

            if (part_count >= 32) break;
        }

        if (saved == 0) break;
        p++;
        while (*p == '/') p++;
    }

    result[0] = '/';
    result[1] = 0;

    for (int i = 0; i < part_count; i++) {
        if (strlen(result) > 1) {
            strncat(result, "/", sizeof(result) - 1 - (int)strlen(result));
        }
        strncat(result, parts[i], sizeof(result) - 1 - (int)strlen(result));
    }

    strncpy(path, result, 255);
    path[255] = 0;
}

void cmd_cd(int argc, char** argv) {
    if (argc < 2) {
        commands_puts("Kullanim: cd <dizin>\n");
        return;
    }

    char path[256];
    build_path(path, sizeof(path), argv[1]);
    normalize_path(path);

    /* FAT pseudo mount */
    if (strncmp(path, "/fat", 4) == 0) {
        const char* fat_sub = path + 4; /* "", "/", "/ASSETS" */

        if (fat_path_exists(fat_sub)) {
            commands_set_cwd(path);
            shell_set_cwd(path);
            return;
        }

        commands_puts("Hata: FAT dizini bulunamadi: ");
        commands_puts(path);
        commands_puts("\n");
        return;
    }

    /* Şimdilik sadece FAT cd destekliyoruz */
    commands_puts("Su anda sadece FAT pseudo-mount icin cd destekleniyor.\n");
}

REGISTER_COMMAND(cd, cmd_cd, "Çalışma dizinini değiştirir");