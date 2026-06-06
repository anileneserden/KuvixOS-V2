#ifndef KUVIX_XHCI_H
#define KUVIX_XHCI_H

#include <stdint.h>
#include <stddef.h>

/**
 * USB Standart Cihaz Tanımlayıcıları
 * Bu yapılar xhci.c içerisinde cihazı tanırken kullanılır.
 */
typedef struct {
    uint8_t  length;
    uint8_t  descriptor_type;
    uint16_t bcd_usb;
    uint8_t  device_class;
    uint8_t  device_subclass;
    uint8_t  device_protocol;
    uint8_t  max_packet_size0;
    uint16_t id_vendor;
    uint16_t id_product;
    uint16_t bcd_device;
    uint8_t  i_manufacturer;
    uint8_t  i_product;
    uint8_t  i_serial_number;
    uint8_t  b_num_configurations;
} __attribute__((packed)) usb_device_descriptor_t;

typedef struct {
    uint8_t  caplength;
    uint8_t  reserved;
    uint16_t hciversion;
    uint32_t hcsparams1;
    uint32_t hcsparams2;
    uint32_t hcsparams3;
    uint32_t hccparams1;
} __attribute__((packed)) xhci_cap_regs_t;

/**
 * xHCI Kontrolcü Yapısı
 * Donanımın temel adresini ve durumunu tutar.
 */
struct xhci_controller {
    uint32_t mmio_base;
    uint32_t slot_count;
    // Diğer xHCI spesifik değişkenler (Ringler vb.) buraya eklenebilir.
};

/**
 * C++ DÜNYASI İLE KÖPRÜ (EXTERN "C")
 * xhci.c içinden veri geldiğinde çağrılacak olan fonksiyon.
 * HIDManager.cpp içinde implemente ettiğimiz fonksiyondur.
 */
#ifdef __cplusplus
extern "C" {
#endif

    // xHCI sürücüsü bir veri paketi aldığında bu C++ fonksiyonunu tetikler
    void usb_hid_cpp_on_report(uint8_t* buf, size_t len);

    // xHCI sürücüsünü başlatan ana fonksiyon
    void xhci_init(uint32_t pci_bar);

#ifdef __cplusplus
}
#endif

#endif