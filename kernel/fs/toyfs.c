#include <kernel/fs/toyfs.h>
#include <stdint.h>

#define MAX_FILES  32
#define MAX_FD     16

typedef struct {
    char name[32];
    uint32_t off;
    uint32_t size;
} toyfs_file_t;

typedef struct {
    int used;
    int file_i;
    uint32_t pos;
} fd_t;

static blockdev_t*   g_dev = 0;
static toyfs_file_t  g_files[MAX_FILES];
static int           g_file_count = 0;
static fd_t          g_fd[MAX_FD];

static uint32_t rd_u32(const uint8_t* p) {
    return (uint32_t)p[0]
        | ((uint32_t)p[1] << 8)
        | ((uint32_t)p[2] << 16)
        | ((uint32_t)p[3] << 24);
}

static int streq(const char* a, const char* b) {
    while (*a && *b) {
        if (*a++ != *b++) return 0;
    }
    return (*a == 0 && *b == 0);
}

static int starts_with(const char* s, const char* pfx)
{
    if (!pfx || !pfx[0]) return 1;
    while (*pfx) {
        if (*s++ != *pfx++) return 0;
    }
    return 1;
}

/* ---- blockdev helpers ---- */
static int dev_read1(uint64_t lba, void* sec512)
{
    if (!g_dev || !g_dev->read) return 0;
    return g_dev->read(g_dev, lba, 1, sec512);
}

/* ---------------------------------------------------- */

int toyfs_mount(blockdev_t* dev)
{
    g_dev = dev;
    g_file_count = 0;
    for (int i = 0; i < MAX_FD; i++) g_fd[i].used = 0;

    if (!g_dev || !g_dev->read) return 0;

    /*
      Header + file table toplam boyut:
      - header: en az 16 byte
      - her entry: 40 byte
      MAX_FILES=32 -> 16 + 32*40 = 1296 byte
      => en az 3 sektör (1536 byte) okumak gerekir.
    */
    uint8_t buf[512 * 3];

    // 3 sektörü tek tek oku (count=1)
    for (int s = 0; s < 3; s++) {
        if (!g_dev->read(g_dev, (uint64_t)s, 1, &buf[s * 512])) {
            return 0;
        }
    }

    // magic: "TOYFS1" (6 char)
    if (!(buf[0]=='T' && buf[1]=='O' && buf[2]=='Y' && buf[3]=='F' && buf[4]=='S' && buf[5]=='1'))
        return 0;

    int fc = (int)rd_u32(&buf[8]);
    if (fc < 0) fc = 0;
    if (fc > MAX_FILES) fc = MAX_FILES;
    g_file_count = fc;

    // file table offset 16, each entry 40 bytes: name[32], off u32, size u32
    uint32_t base = 16;
    for (int i = 0; i < g_file_count; i++) {
        uint32_t o = base + (uint32_t)i * 40u;

        // buf sınırı: 3 sektör => 1536 byte
        if (o + 40u > (uint32_t)sizeof(buf)) {
            // tablo beklenenden büyük; güvenlik için kes
            g_file_count = i;
            break;
        }

        for (int j = 0; j < 32; j++) g_files[i].name[j] = (char)buf[o + (uint32_t)j];
        g_files[i].name[31] = 0; // güvenlik (null-terminate)
        g_files[i].off  = rd_u32(&buf[o + 32]);
        g_files[i].size = rd_u32(&buf[o + 36]);
    }

    return 1;
}

int toyfs_open(const char* path)
{
    if (!g_dev || !path) return -1;

    const char* name = path;
    if (name[0] == '/') name++;

    int fi = -1;
    for (int i = 0; i < g_file_count; i++) {
        if (streq(name, g_files[i].name)) { fi = i; break; }
    }
    if (fi < 0) return -1;

    for (int fd = 0; fd < MAX_FD; fd++) {
        if (!g_fd[fd].used) {
            g_fd[fd].used = 1;
            g_fd[fd].file_i = fi;
            g_fd[fd].pos = 0;
            return fd;
        }
    }
    return -1;
}

int toyfs_read(int fd, void* buf, uint32_t n)
{
    if (fd < 0 || fd >= MAX_FD || !g_fd[fd].used) return 0;

    toyfs_file_t* f = &g_files[g_fd[fd].file_i];

    uint32_t left = (g_fd[fd].pos < f->size) ? (f->size - g_fd[fd].pos) : 0;
    if (n > left) n = left;
    if (n == 0) return 0;

    uint8_t* out = (uint8_t*)buf;
    uint32_t abs = f->off + g_fd[fd].pos;

    // Sector cache: her byte için read yapma
    uint8_t  sec[512];
    uint64_t cur_lba = (uint64_t)-1;

    for (uint32_t i = 0; i < n; i++) {
        uint32_t off = abs + i;
        uint64_t lba = (uint64_t)(off / 512u);
        uint32_t in  = (uint32_t)(off % 512u);

        if (lba != cur_lba) {
            if (!dev_read1(lba, sec)) {
                n = i; // burada okuma bitti
                break;
            }
            cur_lba = lba;
        }

        out[i] = sec[in];
    }

    g_fd[fd].pos += n;
    return (int)n;
}

void toyfs_close(int fd)
{
    if (fd < 0 || fd >= MAX_FD) return;
    g_fd[fd].used = 0;
}

/*
  ✅ Callback'li iter: VFS bununla bağlanacak.
  prefix: "/assets/themes/" gibi gelebilir.
  cb(path,size,u) non-zero dönerse durur.
*/
int toyfs_iter(const char* prefix, toyfs_iter_cb cb, void* u)
{
    if (!cb) return 0;

    const char* pfx = prefix ? prefix : "";
    if (pfx[0] == '/') pfx++;

    char full[64];

    for (int i = 0; i < g_file_count; i++) {
        const char* name = g_files[i].name; // "assets/.."
        if (!starts_with(name, pfx)) continue;

        full[0] = '/';
        int w = 1;
        for (int j = 0; j < 32 && name[j] && w < (int)sizeof(full) - 1; j++) {
            full[w++] = name[j];
        }
        full[w] = 0;

        int r = cb(full, g_files[i].size, u);
        if (r) return r;
    }
    return 0;
}

int toyfs_list(const char* prefix, char* out, uint32_t out_sz)
{
    if (!out || out_sz == 0) return 0;

    const char* pfx = prefix ? prefix : "";
    if (pfx[0] == '/') pfx++;

    uint32_t w = 0;
    for (int i = 0; i < g_file_count; i++) {
        const char* s = g_files[i].name;
        if (!starts_with(s, pfx)) continue;

        while (*s && w + 1 < out_sz) out[w++] = *s++;
        if (w + 1 < out_sz) out[w++] = '\n';
    }
    out[w] = 0;
    return (int)w;
}
