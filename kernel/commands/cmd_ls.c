#include <kernel/fs/vfs.h>
#include <lib/commands.h>
#include <lib/string.h>
#include <kernel/drivers/video/fb_console.h>
#include <kernel/fs/kvxfs.h>
#include <kernel/printk.h>

// vfs_list fonksiyonundan dönecek olan her bir dosya/klasör için çağrılan callback
static int ls_cb(const char* name, uint32_t size, void* u) {
    // GCC'nin 'unused parameter' uyarısını susturmak için
    (void)u;
    (void)size;

    vfs_stat_t st;
    
    // Gelen ismin tipini (klasör mü dosya mı) dinamik olarak sorgula
    if (vfs_stat(name, &st) == 0) {
        if (st.type == VFS_T_DIR) {
            fb_console_set_color(0x000000FF, 0x00000000); // Mavi (Dizin)
        } else {
            fb_console_set_color(0x00FFFFFF, 0x00000000); // Beyaz (Dosya)
        }
    }

    // Dosya/Klasör ismini ekrana bas
    commands_puts(name);
    commands_puts("\n");

    // Konsol rengini varsayılana (beyaz) geri döndür
    fb_console_set_color(0x00FFFFFF, 0x00000000);
    return 0;
}

void cmd_ls(int argc, char** argv) {
    char resolved[VFS_PATH_MAX];
    
    // 1. Yolu çöz (CWD veya dışarıdan gelen parametre)
    const char* input = (argc > 1) ? argv[1] : vfs_get_cwd();
    if (!vfs_resolve_path(input, resolved, sizeof(resolved))) {
        commands_puts("Hata: yol cozumlenemedi.\n");
        return;
    }

    // 2. Önce resmi ve dinamik yol olan VFS katmanını dene
    int ret = vfs_list(resolved, ls_cb, NULL);
    
    // 3. KÖPRÜ KOPUKSA FALLBACK (YEDEK PLAN) DEVREYE GİRER:
    // Eğer VFS katmanı hata dönerse (ret != 0), statik yol kontrolü yapmadan 
    // doğrudan disk okuma fonksiyonunu tetikle!
    if (ret != 0) {
        // Doğrudan disk üzerindeki KVXFS tablolarını dinamik olarak tarar
        kvxfs_list_all(resolved);
    }
}

REGISTER_COMMAND(ls, cmd_ls, "Dizin icerigini listeler");