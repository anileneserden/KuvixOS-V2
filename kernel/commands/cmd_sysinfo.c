// kernel/commands/cmd_sysinfo.c
#include <stdint.h>
#include <kernel/printk.h>
#include <lib/commands.h>
#include <kernel/drivers/video/fb.h>

/**
 * Basit CPUID Wrapper
 * İşlemci bilgilerini (Vendor ID gibi) donanımdan çekmek için kullanılır.
 */
static void cpuid(uint32_t leaf,
                  uint32_t* a,
                  uint32_t* b,
                  uint32_t* c,
                  uint32_t* d)
{
    __asm__ __volatile__(
        "cpuid"
        : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
        : "a"(leaf)
    );
}

void cmd_sysinfo(int argc, char** argv) {
    // Kullanılmayan argümanlar için uyarıları engelle
    (void)argc;
    (void)argv;

    // Başlık ve Sürüm Bilgisi
    printk("KuvixOS V2 (Development Build)\n");
    printk("------------------------------\n");

    // CPU Vendor String (Leaf 0) okuma
    uint32_t eax, ebx, ecx, edx;
    cpuid(0, &eax, &ebx, &ecx, &edx);

    char vendor[13];
    // EBX, EDX ve ECX kayıtçıları vendor string'i (örn: "GenuinIntel") tutar
    vendor[0]  = (char)(ebx & 0xFF);
    vendor[1]  = (char)((ebx >> 8) & 0xFF);
    vendor[2]  = (char)((ebx >> 16) & 0xFF);
    vendor[3]  = (char)((ebx >> 24) & 0xFF);

    vendor[4]  = (char)(edx & 0xFF);
    vendor[5]  = (char)((edx >> 8) & 0xFF);
    vendor[6]  = (char)((edx >> 16) & 0xFF);
    vendor[7]  = (char)((edx >> 24) & 0xFF);

    vendor[8]  = (char)(ecx & 0xFF);
    vendor[9]  = (char)((ecx >> 8) & 0xFF);
    vendor[10] = (char)((ecx >> 16) & 0xFF);
    vendor[11] = (char)((ecx >> 24) & 0xFF);
    vendor[12] = '\0';

    // printk ile formatlı yazdırma
    printk("CPU Vendor : %s\n", vendor);
    printk("CPU Mode   : 32-bit (Protected Mode)\n");

    // Grafik çözünürlük bilgilerini çek (fb.c/gfx.c içinde tanımlıdır)
    uint32_t width = fb_get_width();
    uint32_t height = fb_get_height();
    printk("Display    : %ux%u (LFB)\n", width, height);

    // Zamanlayıcı bilgisi (time.c içinden)
    extern uint32_t g_ticks_ms;
    printk("Kernel Ticks: %u ms\n", g_ticks_ms);
}

REGISTER_COMMAND(sysinfo, cmd_sysinfo, "Sistem ve donanim bilgilerini gosterir");