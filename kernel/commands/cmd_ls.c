#include <kernel/fs/vfs.h>
#include <lib/commands.h>
#include <lib/string.h>
#include <kernel/drivers/video/fb_console.h>
#include <kernel/fs/kvxfs.h>
#include <kernel/printk.h>

typedef struct {
    const char* base_path;
} ls_context_t;

static int ls_cb(const char* name, uint32_t size, void* u) {
    ls_context_t* ctx = (ls_context_t*)u;
    
    // GÜVENLİK FİLTRESİ: Eğer gelen isim listenen klasörün kendi tam yoluyla aynıysa listede gösterme
    if (strcmp(name, ctx->base_path) == 0) {
        return 0; 
    }

    char full_path[VFS_PATH_MAX];
    strcpy(full_path, ctx->base_path);
    size_t len = strlen(full_path);
    if (len > 0 && full_path[len - 1] != '/') {
        strcat(full_path, "/");
    }
    strcat(full_path, name);

    vfs_stat_t st;
    if (vfs_stat(full_path, &st) == 0) {
        if (st.type == VFS_T_DIR) {
            fb_console_set_color(0x000000FF, 0x00000000); // Mavi (Klasör)
        } else {
            fb_console_set_color(0x00FFFFFF, 0x00000000); // Beyaz (Dosya)
        }

        printk("%d bytes  ", st.size);
        commands_puts(name);
        commands_puts("\n");
    }

    fb_console_set_color(0x00FFFFFF, 0x00000000);
    return 0;
}

void cmd_ls(int argc, char** argv) {
    char resolved[VFS_PATH_MAX];
    
    const char* input = (argc > 1) ? argv[1] : vfs_get_cwd();
    if (!input) {
        input = "/";
    }
    
    if (!vfs_resolve_path(input, resolved, sizeof(resolved))) {
        commands_puts("Hata: yol cozumlenemedi.\n");
        return;
    }

    // Kendi yazdığımız printk başlığını kaldırdık, çünkü alt katman otomatik basıyor.
    ls_context_t ctx = { .base_path = resolved };
    vfs_list(resolved, ls_cb, &ctx);
}

REGISTER_COMMAND(ls, cmd_ls, "Dizin icerigini listeler");