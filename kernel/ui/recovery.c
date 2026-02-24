// kernel/ui/recovery.c
#include <kernel/printk.h>
#include <kernel/fs/vfs.h>
#include <kernel/user.h>
#include <stdint.h>

static void cpu_reboot(void) {
    // klasik: 8042 keyboard controller reset
    // (QEMU'da genelde çalışır)
    asm volatile ("cli");
    for (;;) {
        uint8_t good = 0x02;
        while (good & 0x02) {
            asm volatile ("inb $0x64, %0" : "=a"(good));
        }
        asm volatile ("outb %0, $0x64" :: "a"((uint8_t)0xFE));
    }
}

void recovery_delete_and_reboot(const char* path) {
    printk("[RECOVERY] deleting: %s\n", (char*)path);

    int ok = vfs_remove(path); // senin API'n vfs_delete/vfs_remove ise onu kullan
    printk("[RECOVERY] unlink result=%d\n", ok);

    printk("[RECOVERY] rebooting...\n");
    cpu_reboot();
}