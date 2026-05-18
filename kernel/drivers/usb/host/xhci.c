#include <stdint.h>
#include <kernel/printk.h>

void xhci_usb_probe(uint8_t bus, uint8_t slot, uint8_t func) {
    printk("[xHCI] USB 3.0 Host Controller detected at %u:%u:%u. Driver loading soon...\n", 
           bus, slot, func);
}
