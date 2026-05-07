#include <kernel/drivers/usb/hid.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <kernel/printk.h>

// HID aygıtı başlatma
int usb_hid_probe(struct usb_device *udev, uint8_t interface_number, uint8_t protocol, uint8_t subclass) {
    // Sadece klavye veya mouse ise devam et
    if (protocol != USB_HID_PROTOCOL_KEYBOARD && protocol != USB_HID_PROTOCOL_MOUSE)
        return -1;
    // HID aygıtı için yapı ayır
    // struct usb_hid *hid = ...
    // Gerekli alanları doldur
    // ...
    // Gerekirse input subsystem'e kaydet
    // ...
    // Başarılı ise 0 döndür
    return 0;
}

// Yardımcı: Gelen HID raporunu hex olarak logla

static void log_hid_report(const uint8_t *buf, size_t len) {
    printk("[HID] Report:");
    for (size_t i = 0; i < len; ++i) {
        printk(" 0x%02x", (unsigned int)buf[i]);
    }
    printk("\n");
}

// HID raporu okuma (örnek iskelet)
int usb_hid_read_report(struct usb_hid *hid, uint8_t *buf, size_t len) {
    // USB'den rapor oku (interrupt transfer)
    // ...
    // Örnek: gelen veriyi logla
    log_hid_report(buf, len);
    return 0;
}

// Gerekirse ek yardımcı fonksiyonlar
// ...
