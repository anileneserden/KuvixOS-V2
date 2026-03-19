#include <kernel/exec/kef_minimal.h>
#include <kernel/fs/vfs.h>
#include <kernel/printk.h>
#include <lib/string.h>
#include <stdint.h>

static int read_exact(vfs_file_t* f, void* buf, uint32_t size) {
    uint32_t total = 0;

    while (total < size) {
        uint32_t got = 0;

        if (!vfs_read(f, (uint8_t*)buf + total, size - total, &got)) {
            return 0;
        }

        if (got == 0) {
            return 0;
        }

        total += got;
    }

    return 1;
}

int kef_minimal_load_file(const char* path, kef_minimal_app_t* out) {
    if (!path || !out) return 0;

    memset(out, 0, sizeof(*out));

    vfs_file_t* f = 0;
    if (!vfs_open(path, VFS_O_RDONLY, &f) || !f) {
        printk("[KEF] open failed: %s\n", path);
        return 0;
    }

    kef_minimal_header_t hdr;
    memset(&hdr, 0, sizeof(hdr));

    if (!read_exact(f, &hdr, sizeof(hdr))) {
        printk("[KEF] header read failed\n");
        vfs_close(f);
        return 0;
    }

    if (memcmp(hdr.magic, KEF_MINIMAL_MAGIC, 4) != 0) {
        printk("[KEF] bad magic\n");
        vfs_close(f);
        return 0;
    }

    if (hdr.version != KEF_MINIMAL_VERSION) {
        printk("[KEF] bad version=%u\n", hdr.version);
        vfs_close(f);
        return 0;
    }

    if (hdr.window_w == 0 || hdr.window_h == 0) {
        printk("[KEF] invalid window size\n");
        vfs_close(f);
        return 0;
    }

    if (hdr.title_len == 0 || hdr.title_len >= sizeof(out->title)) {
        printk("[KEF] invalid title_len=%u\n", hdr.title_len);
        vfs_close(f);
        return 0;
    }

    if (hdr.text_len == 0 || hdr.text_len >= sizeof(out->text)) {
        printk("[KEF] invalid text_len=%u\n", hdr.text_len);
        vfs_close(f);
        return 0;
    }

    out->width  = (int)hdr.window_w;
    out->height = (int)hdr.window_h;

    if (!read_exact(f, out->title, hdr.title_len)) {
        printk("[KEF] title read failed\n");
        vfs_close(f);
        return 0;
    }
    out->title[hdr.title_len] = 0;

    if (!read_exact(f, out->text, hdr.text_len)) {
        printk("[KEF] text read failed\n");
        vfs_close(f);
        return 0;
    }
    out->text[hdr.text_len] = 0;

    vfs_close(f);

    printk("[KEF] loaded ok: title='%s' text='%s' w=%d h=%d\n",
           out->title, out->text, out->width, out->height);

    return 1;
}