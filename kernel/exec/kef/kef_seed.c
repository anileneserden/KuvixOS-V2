#include <kernel/fs/vfs.h>
#include <kernel/printk.h>
#include <kernel/user.h>
#include <lib/string.h>
#include <stdint.h>

extern const uint8_t _binary_apps_kef_hello_hello_kef_start[];
extern const uint8_t _binary_apps_kef_hello_hello_kef_end[];

static uint32_t hello_kef_size(void) {
    return (uint32_t)(_binary_apps_kef_hello_hello_kef_end - _binary_apps_kef_hello_hello_kef_start);
}

void kef_seed_files(void) {
    (void)vfs_mkdir(USER_APPS_PATH);

    char path[256];
    strcpy(path, USER_APPS_PATH);
    strcat(path, "/hello.kef");

    uint32_t sz = hello_kef_size();

    vfs_file_t* f = 0;
    int orc = vfs_open(path, VFS_O_CREAT | VFS_O_WRONLY, &f);
    printk("[KEF] seed open rc=%d f=%p path=%s\n", orc, f, path);

    // ✅ Senin VFS’te OK = 1
    if (orc == 1 && f) {
        uint32_t nw = 0;
        int wrc = vfs_write(f, _binary_apps_kef_hello_hello_kef_start, sz, &nw);
        printk("[KEF] seed write rc=%d nw=%u want=%u\n", wrc, (unsigned)nw, (unsigned)sz);
        vfs_close(f);
    } else {
        printk("[KEF] seed open failed\n");
    }

    vfs_stat_t st;
    int sr = vfs_stat(path, &st);
    printk("[KEF] seed stat sr=%d type=%d size=%u backend=%d\n",
           sr, st.type, (unsigned)st.size, st.backend);

    uint8_t tmp[64];
    uint32_t out_sz = 0;
    int rr = vfs_read_all(path, tmp, sizeof(tmp), &out_sz);
    printk("[KEF] seed read_all rr=%d out_sz=%u\n", rr, (unsigned)out_sz);
}