#include <kernel/fs/vfs.h>
#include <lib/commands.h>
#include <lib/string.h>
#include <kernel/drivers/video/fb_console.h>
#include <kernel/fs/kvxfs.h>
#include <kernel/printk.h>

typedef struct {
    const char* base_path;
    int detailed;
} ls_context_t;

// Octal izinleri rwx formatına çeviren küçük bir yardımcı fonksiyon (Örn: 0644 -> -rw-r--r--)
static void format_permissions(uint16_t mode, int is_dir, char* out_str) {
    out_str[0] = is_dir ? 'd' : '-';
    
    // Owner (Sahip) izinleri
    out_str[1] = (mode & 0400) ? 'r' : '-';
    out_str[2] = (mode & 0200) ? 'w' : '-';
    out_str[3] = (mode & 0100) ? 'x' : '-';
    
    // Group (Grup) izinleri
    out_str[4] = (mode & 0040) ? 'r' : '-';
    out_str[5] = (mode & 0020) ? 'w' : '-';
    out_str[6] = (mode & 0010) ? 'x' : '-';
    
    // Others (Diğerleri) izinleri
    out_str[7] = (mode & 0004) ? 'r' : '-';
    out_str[8] = (mode & 0002) ? 'w' : '-';
    out_str[9] = (mode & 0001) ? 'x' : '-';
    
    out_str[10] = '\0';
}

static int ls_cb(const char* name, uint32_t size, void* u) {
    ls_context_t* ctx = (ls_context_t*)u;
    
    if (!name || strcmp(name, ctx->base_path) == 0) {
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
    int is_dir = (size == KVX_DIR_SIZE);
    uint16_t mode = 0644; // Varsayılan dosya izni
    uint8_t owner_uid = 0;

    // Eğer vfs_stat başarılı olursa gerçek izinleri, sahibi ve boyutu alalım
    if (vfs_stat(full_path, &st) == 0) {
        is_dir = (st.type == VFS_T_DIR);
        mode = st.permissions;
        size = st.size;
        owner_uid = st.owner_uid;
    }

    if (ctx->detailed) {
        // -l (Detaylı Mod - Linux Tarzı)
        char perm_str[12];
        format_permissions(mode, is_dir, perm_str);

        if (is_dir) {
            fb_console_set_color(0x000000FF, 0x00000000); // Mavi (Klasör)
        } else {
            fb_console_set_color(0x00FFFFFF, 0x00000000); // Beyaz (Dosya)
        }

        // Örnek çıktı formatı: -rw-r--r--   root     128 file.txt
        printk("%s  uid:%d  %6d bytes  ", perm_str, owner_uid, size);
        commands_puts(name);
        commands_puts("\n");
    } else {
        // Normal Mod (Yan yana)
        if (is_dir) {
            fb_console_set_color(0x000000FF, 0x00000000); // Mavi
        } else {
            fb_console_set_color(0x00FFFFFF, 0x00000000); // Beyaz
        }
        commands_puts(name);
        commands_puts("  ");
    }

    fb_console_set_color(0x00FFFFFF, 0x00000000);
    return 0;
}

void cmd_ls(int argc, char** argv) {
    char resolved[VFS_PATH_MAX];
    int detailed = 0;
    const char* target_path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-l") == 0) {
            detailed = 1;
        } else {
            target_path = argv[i];
        }
    }

    const char* input = target_path ? target_path : vfs_get_cwd();
    if (!input) {
        input = "/";
    }
    
    if (!vfs_resolve_path(input, resolved, sizeof(resolved))) {
        commands_puts("Hata: yol cozumlenemedi.\n");
        return;
    }

    ls_context_t ctx = { .base_path = resolved, .detailed = detailed };
    vfs_list(resolved, ls_cb, &ctx);
    
    if (!detailed) {
        commands_puts("\n");
    }
}

REGISTER_COMMAND(ls, cmd_ls, "Dizin icerigini listeler (-l ile detayli)");