#include <kernel/system/zip_reader.h>

#include <kernel/fs/vfs.h>
#include <kernel/printk.h>
#include <lib/string.h>
#include <stdint.h>

#define ZIP_LOCAL_FILE_HEADER_SIG 0x04034B50u

static uint16_t zip_rd16(const uint8_t* p) {
    return (uint16_t)(
        ((uint16_t)p[0]) |
        ((uint16_t)p[1] << 8)
    );
}

static uint32_t zip_rd32(const uint8_t* p) {
    return (uint32_t)(
        ((uint32_t)p[0]) |
        ((uint32_t)p[1] << 8) |
        ((uint32_t)p[2] << 16) |
        ((uint32_t)p[3] << 24)
    );
}

int zip_list_entries(const char* path, zip_entry_callback_t cb, void* user) {
    static uint8_t buf[1024 * 1024];
    uint32_t sz = 0;
    uint32_t off = 0;

    if (!path || !path[0]) {
        printk("[zip] invalid path\n");
        return 0;
    }

    if (!vfs_read_all(path, buf, sizeof(buf), &sz)) {
        printk("[zip] read failed: %s\n", path);
        return 0;
    }

    while (off + 30 <= sz) {
        uint32_t sig = zip_rd32(buf + off);

        if (sig != ZIP_LOCAL_FILE_HEADER_SIG) {
            break;
        }

        {
            zip_entry_t ent;
            uint16_t flags;
            uint16_t method;
            uint32_t crc32;
            uint32_t comp_size;
            uint32_t uncomp_size;
            uint16_t name_len;
            uint16_t extra_len;
            uint32_t name_off;
            uint32_t data_off;
            uint32_t next_off;
            uint32_t copy_len;
            uint32_t i;

            memset(&ent, 0, sizeof(ent));

            flags       = zip_rd16(buf + off + 6);
            method      = zip_rd16(buf + off + 8);
            crc32       = zip_rd32(buf + off + 14);
            comp_size   = zip_rd32(buf + off + 18);
            uncomp_size = zip_rd32(buf + off + 22);
            name_len    = zip_rd16(buf + off + 26);
            extra_len   = zip_rd16(buf + off + 28);

            name_off = off + 30;
            data_off = off + 30 + (uint32_t)name_len + (uint32_t)extra_len;

            if (name_off + (uint32_t)name_len > sz) {
                printk("[zip] invalid filename range\n");
                return 0;
            }

            if (data_off > sz) {
                printk("[zip] invalid data offset\n");
                return 0;
            }

            ent.compressed_size = comp_size;
            ent.uncompressed_size = uncomp_size;
            ent.local_header_offset = off;
            ent.method = method;
            ent.flags = flags;
            ent.crc32 = crc32;
            ent.is_dir = 0;

            copy_len = (uint32_t)name_len;
            if (copy_len >= sizeof(ent.name)) {
                copy_len = sizeof(ent.name) - 1;
            }

            for (i = 0; i < copy_len; i++) {
                ent.name[i] = (char)buf[name_off + i];
            }
            ent.name[copy_len] = '\0';

            if (copy_len > 0 && ent.name[copy_len - 1] == '/') {
                ent.is_dir = 1;
            }

            if (cb) {
                if (!cb(&ent, user)) {
                    return 1;
                }
            }

            next_off = data_off + comp_size;
            if (next_off <= off || next_off > sz) {
                printk("[zip] invalid next offset\n");
                return 0;
            }

            off = next_off;
        }
    }

    return 1;
}