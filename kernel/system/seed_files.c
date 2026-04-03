#include <kernel/system/seed_files.h>

#include <kernel/kef.h>
#include <kernel/user.h>
#include <kernel/fs/vfs.h>
#include <kernel/printk.h>
#include <lib/string.h>
#include <stdint.h>

/* --- helpers --- */

static int file_exists(const char* path) {
    uint8_t tmp[1];
    uint32_t out_sz = 0;
    int r = vfs_read_all(path, tmp, 1, &out_sz);
    return (r == 1); // read_all başarılıysa var say
}

static int write_if_missing(const char* path, const char* text) {
    if (!path || !text) return 0;
    if (file_exists(path)) return 1;

    uint32_t len = (uint32_t)strlen(text);
    int ok = vfs_write_all(path, (const uint8_t*)text, len);
    if (!ok) {
        printk("[seed] write failed: %s\n", path);
        return 0;
    }

    printk("[seed] created: %s (%u bytes)\n", path, (unsigned)len);
    return 1;
}

static int write_blob_if_missing(const char* path, const uint8_t* data, uint32_t size) {
    if (!path || !data || size == 0) return 0;
    if (file_exists(path)) return 1;

    if (!vfs_write_all(path, data, size)) {
        printk("[seed] write failed: %s\n", path);
        return 0;
    }

    printk("[seed] created: %s (%u bytes)\n", path, (unsigned)size);
    return 1;
}

static void seed_hello_kef(void) {
    static const char msg[] = "Hello World from hello.kef\n";
    static const uint8_t code[] = {
        KEF_OP_PRINT,
        0x00, 0x00, 0x00, 0x00,
        KEF_OP_EXIT
    };

    uint8_t blob[128];
    kef_header_t hdr;
    uint32_t total_size;
    char path[128];

    memset(&hdr, 0, sizeof(hdr));
    hdr.magic[0] = KEF_MAGIC_0;
    hdr.magic[1] = KEF_MAGIC_1;
    hdr.magic[2] = KEF_MAGIC_2;
    hdr.magic[3] = KEF_MAGIC_3;
    hdr.version = KEF_VERSION_1;
    hdr.app_kind = KEF_APP_TERMINAL;
    hdr.code_offset = (uint32_t)sizeof(kef_header_t);
    hdr.code_size = (uint32_t)sizeof(code);
    hdr.str_offset = hdr.code_offset + hdr.code_size;
    hdr.str_size = (uint32_t)sizeof(msg);

    total_size = hdr.str_offset + hdr.str_size;
    if (total_size > sizeof(blob)) return;

    memset(blob, 0, sizeof(blob));
    memcpy(blob, &hdr, sizeof(hdr));
    memcpy(blob + hdr.code_offset, code, sizeof(code));
    memcpy(blob + hdr.str_offset, msg, sizeof(msg));

    vfs_mkdir("/home");
    vfs_mkdir(USER_HOME_PATH);
    vfs_mkdir(USER_DESKTOP_PATH);

    memset(path, 0, sizeof(path));
    strncpy(path, USER_DESKTOP_PATH, sizeof(path) - 1);
    strncat(path, "/hello.kef", sizeof(path) - 1 - (int)strlen(path));

    write_blob_if_missing(path, blob, total_size);
}

/* --- seeds --- */

typedef struct {
    const char* path;
    const char* content;
} seed_text_t;

static const seed_text_t k_seed_texts[] = {
    {
        "/etc/vhost.conf",
        "deneme.local /home/anil/html/deneme/",
        "home.local /home/anil/html/home/"
    },
    {
        "/home/anil/html/deneme/index.html",
        "<h1>Deneme Local</h1>",
        "<p>KuvixBrowser local vhost test</p>"
    }
};

/* --- entry --- */

void seed_files_run(void) {
    printk("[seed] run\n");

    for (uint32_t i = 0; i < (uint32_t)(sizeof(k_seed_texts) / sizeof(k_seed_texts[0])); i++) {
        write_if_missing(k_seed_texts[i].path, k_seed_texts[i].content);
    }

    seed_hello_kef();

    printk("[seed] done\n");
}