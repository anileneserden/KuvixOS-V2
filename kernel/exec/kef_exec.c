// kernel/exec/kef_exec.c
#include <kernel/exec/kef_exec.h>

#include <kernel/fs/vfs.h>
#include <kernel/memory/kmalloc.h>
#include <kernel/printk.h>
#include <lib/string.h>
#include <stdint.h>

/* SDK tarafındaki C ABI */
#include <kvx_c/kuvixos.h>

/* mevcut JSON loader */
#include <kernel/exec/kef_json.h>

#include <ui/wm.h>

/* --------------------------------------------------
 * Minimal ELF32 loader for KEF v1
 * -------------------------------------------------- */

typedef struct {
    uint8_t  e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint32_t e_entry;
    uint32_t e_phoff;
    uint32_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} elf32_ehdr_t;

typedef struct {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
} elf32_phdr_t;

#define ELF_MAGIC0 0x7F
#define ELF_MAGIC1 'E'
#define ELF_MAGIC2 'L'
#define ELF_MAGIC3 'F'

#define PT_LOAD 1

/* --------------------------------------------------
 * Dummy draw API for first version
 * Sonra bunu gerçek pencere draw API'sine bağlarız.
 * -------------------------------------------------- */

static void kef_api_fill_rect(int x, int y, int w, int h, uint32_t color) {
    (void)x; (void)y; (void)w; (void)h; (void)color;
    /* şimdilik boş */
}

static void kef_api_text(int x, int y, uint32_t color, const char* s) {
    (void)x; (void)y; (void)color; (void)s;
    /* şimdilik boş */
}

static kvx_api_t g_kef_api = {
    .fill_rect = kef_api_fill_rect,
    .text = kef_api_text,
};

/* -------------------------------------------------- */

static int elf32_is_valid(const elf32_ehdr_t* eh, uint32_t size) {
    if (!eh) return 0;
    if (size < sizeof(elf32_ehdr_t)) return 0;

    if (eh->e_ident[0] != ELF_MAGIC0) return 0;
    if (eh->e_ident[1] != ELF_MAGIC1) return 0;
    if (eh->e_ident[2] != ELF_MAGIC2) return 0;
    if (eh->e_ident[3] != ELF_MAGIC3) return 0;

    if (eh->e_phoff == 0) return 0;
    if (eh->e_phnum == 0) return 0;
    if (eh->e_phentsize != sizeof(elf32_phdr_t)) return 0;

    return 1;
}

int kef_exec_file(const char* path, kef_minimal_state_t* st) {
    uint8_t* file_data = 0;
    uint32_t file_size = 0;

    if (!path || !st) {
        printk("[KEF] invalid args\n");
        return 0;
    }

    printk("[KEF] exec: %s\n", path);

    if (!vfs_read_all_alloc(path, &file_data, &file_size)) {
        printk("[KEF] read failed: %s\n", path);
        return 0;
    }

    elf32_ehdr_t* eh = (elf32_ehdr_t*)file_data;
    if (!elf32_is_valid(eh, file_size)) {
        printk("[KEF] invalid ELF: %s\n", path);
        kfree(file_data);
        return 0;
    }

    printk("[KEF] ELF ok entry=0x%X phoff=%u phnum=%u\n",
           eh->e_entry, eh->e_phoff, eh->e_phnum);

    elf32_phdr_t* ph = (elf32_phdr_t*)(file_data + eh->e_phoff);

    for (uint16_t i = 0; i < eh->e_phnum; i++) {
        elf32_phdr_t* p = &ph[i];

        if (p->p_type != PT_LOAD) continue;

        if (p->p_offset + p->p_filesz > file_size) {
            printk("[KEF] bad segment bounds\n");
            kfree(file_data);
            return 0;
        }

        void* dst = (void*)(uintptr_t)p->p_vaddr;
        void* src = (void*)(file_data + p->p_offset);

        printk("[KEF] load seg[%u] vaddr=0x%X filesz=%u memsz=%u\n",
               (unsigned)i, p->p_vaddr, p->p_filesz, p->p_memsz);

        memcpy(dst, src, p->p_filesz);

        if (p->p_memsz > p->p_filesz) {
            memset((uint8_t*)dst + p->p_filesz, 0, p->p_memsz - p->p_filesz);
        }
    }

    typedef int (*kef_entry_fn_t)(const kvx_api_t* api, kvx_kef_app_t* out_vtbl);
    kef_entry_fn_t entry = (kef_entry_fn_t)(uintptr_t)eh->e_entry;

    kvx_kef_app_t app;
    memset(&app, 0, sizeof(app));

    printk("[KEF] calling entry...\n");

    int rc = entry(&g_kef_api, &app);

    printk("[KEF] entry returned rc=%d\n", rc);
    printk("[KEF] ui_json ptr=%u size=%u\n",
        (uint32_t)(uintptr_t)app.ui_json,
        (uint32_t)app.ui_json_size);

    if (app.ui_json) {
        const unsigned char* p = (const unsigned char*)app.ui_json;
        printk("[KEF] first bytes: %u %u %u %u\n",
            (unsigned)p[0], (unsigned)p[1], (unsigned)p[2], (unsigned)p[3]);
    }

    if (!app.ui_json || app.ui_json_size == 0) {
        printk("[KEF] ui_json missing\n");
        kfree(file_data);
        return 0;
    }

    /* Embedded JSON'u geçici dosyaya döküp mevcut parser ile yükle */
    const char* tmp_path = "/tmp/kef_runtime.json";

    if (!vfs_write_all(tmp_path, (const uint8_t*)app.ui_json, app.ui_json_size)) {
        printk("[KEF] failed to write temp ui json\n");
        kfree(file_data);
        return 0;
    }

    printk("[KEF] ui json dumped to %s (%u bytes)\n",
           tmp_path, (unsigned)app.ui_json_size);

    /* mevcut pencere kimliğini kaybetme */
    int win_id = st->window_id;

    /* state'i temizle ama pencereyi koru */
    memset(st, 0, sizeof(*st));
    st->window_id = win_id;

    if (!kef_json_load_file(tmp_path, st)) {
        printk("[KEF] json load from temp failed\n");
        kfree(file_data);
        return 0;
    }

    st->loaded = 1;

    wm_set_title(st->window_id, st->title);
    wm_set_window_size(st->window_id, st->width, st->height);
    wm_invalidate_window(st->window_id);

    kfree(file_data);
    return 1;
}