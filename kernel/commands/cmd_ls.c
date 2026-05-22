#include <kernel/fs/vfs.h>
#include <lib/commands.h>
#include <lib/string.h>
#include <kernel/drivers/video/fb_console.h>
#include <kernel/fs/kvxfs.h>
#include <kernel/printk.h>

// Callback yapısı
typedef struct {
    const char* base_path;
} ls_context_t;

static int ls_cb(const char* name, uint32_t size, void* u) {
    (void)size;
    ls_context_t* ctx = (ls_context_t*)u;
    
    char full_path[VFS_PATH_MAX];
    
    // Manuel Path Birleştirme (Kendi string.c fonksiyonlarını kullanarak)
    strcpy(full_path, ctx->base_path);
    
    // Eğer yolun sonunda / yoksa ekle
    size_t len = strlen(full_path);
    if (len > 0 && full_path[len - 1] != '/') {
        strcat(full_path, "/");
    }
    strcat(full_path, name);

    vfs_stat_t st;
    
    // Dosya tipine göre renk belirleme
    if (vfs_stat(full_path, &st) == 0) {
        if (st.type == VFS_T_DIR) {
            // Mavi (Dizin)
            fb_console_set_color(0x000000FF, 0x00000000); 
        } else {
            // Beyaz (Dosya)
            fb_console_set_color(0x00FFFFFF, 0x00000000); 
        }
    }

    commands_puts(name);
    commands_puts("\n");

    // Rengi varsayılana döndür (Beyaz)
    fb_console_set_color(0x00FFFFFF, 0x00000000);
    return 0;
}

void cmd_ls(int argc, char** argv) {
    char resolved[VFS_PATH_MAX];
    
    // Yolu belirle (Parametre varsa onu, yoksa CWD'yi al)
    const char* input = (argc > 1) ? argv[1] : vfs_get_cwd();
    
    if (!vfs_resolve_path(input, resolved, sizeof(resolved))) {
        commands_puts("Hata: yol cozumlenemedi.\n");
        return;
    }

    ls_context_t ctx = { .base_path = resolved };

    // VFS listeyi dene
    if (vfs_list(resolved, ls_cb, &ctx) != 0) {
        // Hata durumunda KVXFS fallback
        kvxfs_list_all(resolved);
    }
}

REGISTER_COMMAND(ls, cmd_ls, "Dizin icerigini listeler");