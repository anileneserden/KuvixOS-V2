#include <stdint.h>
#include <kernel/printk.h>

void hda_audio_probe(uint8_t bus, uint8_t slot, uint8_t func) {
    printk("[HDA] Intel HD Audio Controller detected at %u:%u:%u. Driver loading soon...\n", 
           bus, slot, func);
}
