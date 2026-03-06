#include <kernel/drivers/net/net.h>
#include <kernel/drivers/net/pci.h>
#include <kernel/drivers/net/e1000.h>
#include <kernel/printk.h>

void net_init(void) {
    printk("[NET] net_init()\n");
    pci_scan_dump_nics();
}