#include <kernel/fs/vfs.h>
#include <lib/commands.h>
#include <lib/string.h>
#include <kernel/drivers/video/fb_console.h>
#include <kernel/fs/kvxfs.h>
#include <kernel/printk.h>

// Context yapısı: Dosyanın tam yolunu oluşturmak için gerekli veriyi taşır
typedef struct {
    const char* base_path;
} ls_context_t;

static int ls_cb(const char* name, uint32_t size, void* u) {
    (void)size;
    ls_context_t* ctx = (ls_context_t*)u;
    
    char full_path[VFS_PATH_MAX];
    // Tam yolu oluştur: /home/anil + / + desktop
    vfs_join_path(ctx->base_path, name, full_path, sizeof(full_path));

    vfs_stat_t st;
    
    // Artık tam yolu (full_path) stat ediyoruz
    if (vfs_stat(full_path, &st) == 0) {
        if (st.type == VFS_T_DIR) {
            fb_console_set_color(0x000000FF, 0x00000000); // Mavi (Dizin)
        } else {
            fb_console_set_color(0x00FFFFFF, 0x00000000); // Beyaz (Dosya)
        }
    }

    commands_puts(name);
    commands_puts("\n");

    fb_console_set_color(0x00FFFFFF, 0x00000000); // Reset
    return 0;
}

void cmd_ls(int argc, char** argv) {
    char resolved[VFS_PATH_MAX];
    
    const char* input = (argc > 1) ? argv[1] : vfs_get_cwd();
    
    if (!vfs_resolve_path(input, resolved, sizeof(resolved))) {
        commands_puts("Hata: yol cozumlenemedi.\n");
        return;
    }

    // Callback'e tam yolu iletmek için context oluşturuyoruz
    ls_context_t ctx = { .base_path = resolved };

    // VFS katmanını dene
    if (vfs_list(resolved, ls_cb, &ctx) != 0) {
        // Fallback: KVXFS doğrudan tarama
        kvxfs_list_all(resolved);
    }
}

REGISTER_COMMAND(ls, cmd_ls, "Dizin icerigini listeler");