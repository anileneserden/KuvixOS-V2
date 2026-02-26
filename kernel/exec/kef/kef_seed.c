#include <kernel/fs/vfs.h>
#include <kernel/printk.h>
#include <kernel/user.h>
#include <lib/string.h>
#include <stdint.h>

#define VFS_OK(rc) ((rc) != 0)

// objcopy -I binary ile gömülen hello.kef sembolleri
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

    if (VFS_OK(orc) && f) {
        uint32_t nw = 0;
        int wrc = vfs_write(f, _binary_apps_kef_hello_hello_kef_start, sz, &nw);
        printk("[KEF] seed write rc=%d nw=%u want=%u\n", wrc, (unsigned)nw, (unsigned)sz);
        vfs_close(f);
    } else {
        printk("[KEF] seed open failed (orc=%d f=%p)\n", orc, f);
    }

    vfs_stat_t st;
    int sr = vfs_stat(path, &st);
    if (!VFS_OK(sr)) {
        printk("[KEF] seed stat FAILED sr=%d path=%s\n", sr, path);
    } else {
        printk("[KEF] seed stat OK sr=%d type=%d size=%u backend=%d\n",
               sr, st.type, (unsigned)st.size, st.backend);
    }

    uint8_t tmp[64];
    uint32_t out_sz = 0;
    int rr = vfs_read_all(path, tmp, sizeof(tmp), &out_sz);
    if (!VFS_OK(rr)) {
        printk("[KEF] seed read_all FAILED rr=%d out_sz=%u path=%s\n",
               rr, (unsigned)out_sz, path);
    } else {
        printk("[KEF] seed read_all OK rr=%d out_sz=%u\n", rr, (unsigned)out_sz);
    }
}