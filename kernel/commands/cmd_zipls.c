#include <kernel/system/zip_reader.h>
#include <lib/commands.h>

static int zipls_cb(const zip_entry_t* e, void* user) {
    (void)user;

    commands_printf(
        "name=%s method=%u comp=%u uncomp=%u dir=%d\n",
        e->name,
        (unsigned)e->method,
        (unsigned)e->compressed_size,
        (unsigned)e->uncompressed_size,
        e->is_dir
    );

    return 1;
}

void cmd_zipls(int argc, char** argv) {
    if (argc < 2 || !argv[1] || !argv[1][0]) {
        commands_puts("Kullanim: zipls <arsiv_yolu>\n");
        return;
    }

    commands_puts("ZIP entries:\n");

    if (!zip_list_entries(argv[1], zipls_cb, 0)) {
        commands_puts("zipls: arsiv okunamadi veya gecersiz\n");
        return;
    }

    commands_puts("zipls: tamam\n");
}

REGISTER_COMMAND(zipls, cmd_zipls, "ZIP/KShell arsivinin icindeki dosyalari listeler");