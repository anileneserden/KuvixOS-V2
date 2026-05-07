#ifndef _KERNEL_USB_HID_H_
#define _KERNEL_USB_HID_H_

#include <stdint.h>
#include <stddef.h>

#define USB_HID_CLASS_CODE 0x03
#define USB_HID_SUBCLASS_BOOT 0x01
#define USB_HID_PROTOCOL_KEYBOARD 0x01
#define USB_HID_PROTOCOL_MOUSE 0x02

struct usb_device;

// HID aygıtı için temel yapı
struct usb_hid {
    struct usb_device *udev;
    uint8_t protocol;
    uint8_t subclass;
    uint8_t interface_number;
    // Ek alanlar: rapor uzunluğu, buffer, vs.
};

// HID aygıtı başlatma
int usb_hid_probe(struct usb_device *udev, uint8_t interface_number, uint8_t protocol, uint8_t subclass);

// HID raporu okuma (örnek iskelet)
int usb_hid_read_report(struct usb_hid *hid, uint8_t *buf, size_t len);

#endif // _KERNEL_USB_HID_H_
