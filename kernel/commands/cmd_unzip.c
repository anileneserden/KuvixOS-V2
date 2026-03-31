#include <kernel/system/zip_reader.h>
#include <kernel/fs/vfs.h>
#include <lib/commands.h>
#include <lib/string.h>
#include <kernel/printk.h>
#include <stdint.h>

typedef struct {
    const char* zip_path;
    const char* target_dir;
    int ok;
    int extracted_files;
    int created_dirs;
} unzip_ctx_t;

static void path_join(char* out, int out_sz, const char* a, const char* b) {
    int i = 0;
    int j = 0;

    if (!out || out_sz <= 0) return;
    out[0] = '\0';

    if (!a) a = "";
    if (!b) b = "";

    while (a[i] && i < out_sz - 1) {
        out[i] = a[i];
        i++;
    }
    out[i] = '\0';

    if (i > 0 && out[i - 1] != '/' && i < out_sz - 1) {
        out[i++] = '/';
        out[i] = '\0';
    }

    while (b[j] == '/') j++;

    while (b[j] && i < out_sz - 1) {
        out[i++] = b[j++];
    }
    out[i] = '\0';
}

static void mkdirs_for_path(const char* path) {
    char tmp[512];
    int i;

    if (!path || !path[0]) return;

    memset(tmp, 0, sizeof(tmp));
    strncpy(tmp, path, sizeof(tmp) - 1);

    for (i = 1; tmp[i]; i++) {
        if (tmp[i] == '/') {
            tmp[i] = '\0';
            vfs_mkdir(tmp);
            tmp[i] = '/';
        }
    }
}

static int unzip_cb(const zip_entry_t* e, void* user) {
    unzip_ctx_t* ctx = (unzip_ctx_t*)user;
    char out_path[512];

    if (!ctx || !e) return 0;

    path_join(out_path, sizeof(out_path), ctx->target_dir, e->name);

    if (e->is_dir) {
        mkdirs_for_path(out_path);
        vfs_mkdir(out_path);
        ctx->created_dirs++;
        commands_printf("dir  %s\n", out_path);
        return 1;
    }

    {
        static uint8_t file_buf[1024 * 1024];
        uint32_t file_len = 0;

        mkdirs_for_path(out_path);

        if (!zip_extract_stored_entry(ctx->zip_path, e, file_buf, sizeof(file_buf), &file_len)) {
            commands_printf("hata: extract basarisiz: %s\n", e->name);
            ctx->ok = 0;
            return 0;
        }

        if (!vfs_write_all(out_path, file_buf, file_len)) {
            commands_printf("hata: yazma basarisiz: %s\n", out_path);
            ctx->ok = 0;
            return 0;
        }

        ctx->extracted_files++;
        commands_printf("file %s (%u byte)\n", out_path, (unsigned)file_len);
    }

    return 1;
}

void cmd_unzip(int argc, char** argv) {
    const char* zip_path = 0;
    const char* target_dir = 0;
    int i;

    unzip_ctx_t ctx;

    if (argc < 2) {
        commands_puts("Kullanim: unzip <arsiv> [-d hedef]\n");
        commands_puts("Ornek: unzip /persist/kuvix-modern.kshell -d /system/kui/shells/modern\n");
        return;
    }

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0) {
            if (i + 1 < argc) {
                target_dir = argv[i + 1];
                i++;
            } else {
                commands_puts("Hata: -d icin hedef klasor gerekli\n");
                return;
            }
        } else if (!zip_path) {
            zip_path = argv[i];
        }
    }

    if (!zip_path) {
        commands_puts("Hata: arsiv yolu eksik\n");
        return;
    }

    if (!target_dir) {
        target_dir = "/home/anil";
    }

    memset(&ctx, 0, sizeof(ctx));
    ctx.zip_path = zip_path;
    ctx.target_dir = target_dir;
    ctx.ok = 1;

    mkdirs_for_path(target_dir);
    vfs_mkdir(target_dir);

    commands_printf("Extracting %s -> %s\n", zip_path, target_dir);

    if (!zip_list_entries(zip_path, unzip_cb, &ctx) || !ctx.ok) {
        commands_puts("unzip: hata\n");
        return;
    }

    commands_printf("unzip: tamam files=%d dirs=%d\n",
                    ctx.extracted_files,
                    ctx.created_dirs);
}

REGISTER_COMMAND(unzip, cmd_unzip, "ZIP/KShell arsivini klasore cikarir. Ornek: unzip file.kshell -d /target");