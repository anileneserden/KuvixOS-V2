#include <kernel/drivers/usb/xhci.h>
#include <kernel/printk.h>
#include <kernel/drivers/pci/pci.h>

void xhci_init(uint32_t pci_bar) {
    printk("[XHCI] XHCI Surucusu baslatiliyor...\n");
    printk("[XHCI] Base Address (Raw): 0x%x\n", pci_bar);

    // DİKKAT: Burada donanımı bir de pci_read32 ile okumayı deneyelim.
    // Eğer pci_read32 ile değerleri alabiliyorsak, sorun MMIO'da değil,
    // bizim o adresi pointer olarak okuyamamamızdadır (Mapping sorunu).
    
    // xHCI Capability Registers (Capability Length ve Interface Version 0x00 offset'tedir)
    // Bus=0, Slot=4 olduğu biliniyor.
    uint32_t val = pci_read32(0, 4, 0, 0x00); 
    
    printk("[XHCI] PCI Config Okumasi (Offset 0): 0x%x\n", val);
    
    // Eğer buradan da 0 geliyorsa, donanım register'ları hala kilitlidir.
}