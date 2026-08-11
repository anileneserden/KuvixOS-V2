#include <kernel/fs/vfs.h>
#include <lib/commands.h>
#include <kernel/fs/kvxfs.h>
#include <kernel/exec/kef.h>
#include <kernel/exec/task.h> // bg_task_t için
#include <kernel/memory/kmalloc.h>
#include <lib/string.h>
#include <stdint.h>

#define KEF_RELOC_MAX 256

// Arka plan görev listesi ve ID sayacı
bg_task_t g_bg_tasks[MAX_BACKGROUND_TASKS];
uint32_t g_next_task_id = 1;

static void kef_print(const char* s) {
    commands_puts(s);
}

static void cmd_exec(int argc, char** argv) {
    if (argc < 2) {
        commands_puts("kullanim: exec <yol.kef> [&]\n");
        return;
    }

    const char* raw_path = NULL;
    int is_background = 0;

    // Argümanları esnek bir şekilde tarayalım (& işaretini ve dosya yolunu güvenle ayırt edelim)
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "&") == 0) {
            is_background = 1;
        } else if (raw_path == NULL) {
            raw_path = argv[i];
        }
    }

    if (!raw_path) {
        commands_puts("kullanim: exec <yol.kef> [&]\n");
        return;
    }

    // Eğer yolun bitişinde boşluksuz "&" birleşik kaldıysa temizleyelim
    char path_buf[VFS_PATH_MAX] = {0};
    strncpy(path_buf, raw_path, sizeof(path_buf) - 1);
    size_t rlen = strlen(path_buf);
    if (rlen > 0 && path_buf[rlen - 1] == '&') {
        path_buf[rlen - 1] = '\0';
        is_background = 1;
    }
    const char* path = path_buf;

    char target_path[VFS_PATH_MAX] = {0};
    if (path[0] == '/') {
        strncpy(target_path, path, VFS_PATH_MAX - 1);
    } else {
        const char* current_cwd = vfs_get_cwd();
        if (!current_cwd) current_cwd = "/";
        strncpy(target_path, current_cwd, VFS_PATH_MAX - 1);
        size_t len = strlen(target_path);
        if (len > 0 && target_path[len - 1] != '/') strcat(target_path, "/");
        strcat(target_path, path);
    }

    // --- 1. Header'i oku ---
    kef_header_t hdr;
    uint32_t nread = 0;
    if (!kvxfs_read_at(target_path, &hdr, 0, sizeof(hdr), &nread) || nread != sizeof(hdr)) {
        commands_puts("exec: header okunamadi: ");
        commands_puts(target_path);
        commands_puts("\n");
        return;
    }

    if (hdr.magic[0] != KEF_MAGIC0 || hdr.magic[1] != KEF_MAGIC1 ||
        hdr.magic[2] != KEF_MAGIC2 || hdr.version != KEF_VERSION ||
        hdr.arch != KEF_ARCH_X86_32) {
        commands_puts("exec: gecersiz .kef formati\n");
        return;
    }

    if (hdr.reloc_count > KEF_RELOC_MAX) {
        commands_puts("exec: relocation sayisi cok fazla\n");
        return;
    }

    // --- 2. Bellek ayir ---
    uint32_t total_size = hdr.code_size + hdr.bss_size;
    uint8_t* buf = (uint8_t*)kmalloc(total_size);
    if (!buf) {
        commands_puts("exec: bellek ayrilamadi\n");
        return;
    }

    // --- 3. Kod blogunu oku, bss'i sifirla ---
    if (hdr.code_size > 0) {
        if (!kvxfs_read_at(target_path, buf, sizeof(hdr), hdr.code_size, &nread) ||
            nread != hdr.code_size) {
            commands_puts("exec: kod blogu okunamadi\n");
            kfree(buf);
            return;
        }
    }
    if (hdr.bss_size > 0) {
        memset(buf + hdr.code_size, 0, hdr.bss_size);
    }

    // --- 4. Relocation tablosunu oku ---
    kef_reloc_t relocs[KEF_RELOC_MAX];
    if (hdr.reloc_count > 0) {
        uint32_t reloc_bytes = hdr.reloc_count * sizeof(kef_reloc_t);
        if (!kvxfs_read_at(target_path, relocs, hdr.reloc_off, reloc_bytes, &nread) ||
            nread != reloc_bytes) {
            commands_puts("exec: relocation tablosu okunamadi\n");
            kfree(buf);
            return;
        }
    }

    // --- 5. Delta'yi hesapla ve uygula ---
    int32_t delta = (int32_t)((uintptr_t)buf - hdr.preferred_addr);
    for (uint32_t i = 0; i < hdr.reloc_count; i++) {
        if (relocs[i].code_offset + 4 > hdr.code_size) {
            commands_puts("exec: gecersiz relocation offseti\n");
            kfree(buf);
            return;
        }
        uint32_t* slot = (uint32_t*)(buf + relocs[i].code_offset);
        *slot = (uint32_t)((int32_t)(*slot) + delta);
    }

    kef_api_t api;
    api.print = kef_print;
    kef_entry_fn entry = (kef_entry_fn)(buf + hdr.entry_off);

    // --- 6. Arka plan veya Ön plan kontrolü ---
    if (is_background) {
        int saved = 0;
        for (int i = 0; i < MAX_BACKGROUND_TASKS; i++) {
            if (!g_bg_tasks[i].active) {
                g_bg_tasks[i].id = g_next_task_id++;
                g_bg_tasks[i].buffer = buf;
                strncpy(g_bg_tasks[i].path, target_path, VFS_PATH_MAX - 1);
                g_bg_tasks[i].active = 1;
                saved = 1;
                break;
            }
        }

        if (saved) {
            commands_puts("[i] Surec arka plana atildi.\n");
        } else {
            commands_puts("exec: maksimum arka plan gorev siniri!\n");
            kfree(buf);
        }
    } else {
        commands_printf("exec: preferred=%x actual=%p delta=%d\n",
                     hdr.preferred_addr, buf, delta);
        entry(&api);
        kfree(buf);
        commands_puts("\n");
    }
}

REGISTER_COMMAND(exec, cmd_exec, "exec <yol.kef> [&] - Bir .kef dosyasini calistirir");