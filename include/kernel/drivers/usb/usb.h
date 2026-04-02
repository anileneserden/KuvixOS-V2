#pragma once

#include <stdint.h>

enum {
    USB_REQ_GET_STATUS        = 0,
    USB_REQ_CLEAR_FEATURE     = 1,
    USB_REQ_SET_FEATURE       = 3,
    USB_REQ_SET_ADDRESS       = 5,
    USB_REQ_GET_DESCRIPTOR    = 6,
    USB_REQ_SET_DESCRIPTOR    = 7,
    USB_REQ_GET_CONFIGURATION = 8,
    USB_REQ_SET_CONFIGURATION = 9,
    USB_REQ_GET_INTERFACE     = 10,
    USB_REQ_SET_INTERFACE     = 11,
};

enum {
    USB_DESC_DEVICE        = 1,
    USB_DESC_CONFIGURATION = 2,
    USB_DESC_STRING        = 3,
    USB_DESC_INTERFACE     = 4,
    USB_DESC_ENDPOINT      = 5,
    USB_DESC_HID           = 0x21,
    USB_DESC_REPORT        = 0x22,
};

enum {
    USB_EP_ATTR_CONTROL     = 0,
    USB_EP_ATTR_ISOCHRONOUS = 1,
    USB_EP_ATTR_BULK        = 2,
    USB_EP_ATTR_INTERRUPT   = 3,
};

enum {
    USB_CLASS_HID          = 3,
    USB_CLASS_MASS_STORAGE = 8,
};

enum {
    USB_MSC_SUBCLASS_SCSI_TRANSPARENT = 6,
    USB_MSC_PROTOCOL_BULK_ONLY        = 0x50,
};

typedef struct __attribute__((packed)) {
    uint8_t  bmRequestType;
    uint8_t  bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} usb_setup_packet_t;

typedef struct __attribute__((packed)) {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t bcdUSB;
    uint8_t  bDeviceClass;
    uint8_t  bDeviceSubClass;
    uint8_t  bDeviceProtocol;
    uint8_t  bMaxPacketSize0;
    uint16_t idVendor;
    uint16_t idProduct;
    uint16_t bcdDevice;
    uint8_t  iManufacturer;
    uint8_t  iProduct;
    uint8_t  iSerialNumber;
    uint8_t  bNumConfigurations;
} usb_device_descriptor_t;

typedef struct __attribute__((packed)) {
    uint8_t bLength;
    uint8_t bDescriptorType;
} usb_descriptor_header_t;

typedef struct __attribute__((packed)) {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint16_t wTotalLength;
    uint8_t  bNumInterfaces;
    uint8_t  bConfigurationValue;
    uint8_t  iConfiguration;
    uint8_t  bmAttributes;
    uint8_t  bMaxPower;
} usb_configuration_descriptor_t;

typedef struct __attribute__((packed)) {
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bInterfaceNumber;
    uint8_t bAlternateSetting;
    uint8_t bNumEndpoints;
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t iInterface;
} usb_interface_descriptor_t;

typedef struct __attribute__((packed)) {
    uint8_t  bLength;
    uint8_t  bDescriptorType;
    uint8_t  bEndpointAddress;
    uint8_t  bmAttributes;
    uint16_t wMaxPacketSize;
    uint8_t  bInterval;
} usb_endpoint_descriptor_t;

typedef struct __attribute__((packed)) {
    uint32_t dCBWSignature;
    uint32_t dCBWTag;
    uint32_t dCBWDataTransferLength;
    uint8_t  bmCBWFlags;
    uint8_t  bCBWLUN;
    uint8_t  bCBWCBLength;
    uint8_t  CBWCB[16];
} usb_msc_cbw_t;

typedef struct __attribute__((packed)) {
    uint32_t dCSWSignature;
    uint32_t dCSWTag;
    uint32_t dCSWDataResidue;
    uint8_t  bCSWStatus;
} usb_msc_csw_t;