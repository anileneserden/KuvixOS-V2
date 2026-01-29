#include <kernel/fs.h>
#include <kernel/kmalloc.h>
#include <kernel/printk.h>
#include <lib/string.h>
#include <lib/commands.h>

// ramfs.c'deki fonksiyonu prototip olarak bildiriyoruz
extern uint32_t ramfs_read(fs_node_t*, uint32_t, uint32_t, uint8_t*);

void cmd_echo(int argc, char** argv) {
    if (argc < 3) {
        printk("Kullanim: echo <dosya_adi> <mesaj>\n");
        return;
    }

    char *filename = argv[1];
    char *message = argv[2];
    uint32_t len = strlen(message);

    // 1. Yeni dosya düğümü oluştur
    fs_node_t *node = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    memset(node, 0, sizeof(fs_node_t));

    // 2. Mesaj için yer ayır ve kopyala
    node->data = (uintptr_t)kmalloc(len + 1);
    strcpy((char*)node->data, message);

    // 3. Bilgileri doldur
    strcpy(node->name, filename);
    node->length = len;
    node->flags = FS_FILE;
    node->read = &ramfs_read; // Okuma fonksiyonunu bağladık

    // 4. Listeye ekle
    node->next = fs_root->next;
    fs_root->next = node;

    printk("'%s' dosyasina yazildi: %s\n", filename, message);
}

REGISTER_COMMAND(echo, cmd_echo, "Dosyaya metin yazar");