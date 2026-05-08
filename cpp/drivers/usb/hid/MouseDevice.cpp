#include <cpp/drivers/usb/hid/MouseDevice.hpp>
#include <kernel/printk.h>

// Kernel'deki global koordinatlar (Pineapple DE'nin okuduğu değişkenler)
extern "C" int mouse_x;
extern "C" int mouse_y;

namespace Kuvix::Drivers::USB::HID {

    MouseDevice::MouseDevice() {
        state.leftButton = false;
        state.rightButton = false;
        state.middleButton = false;
    }

    void MouseDevice::handleReport(uint8_t* buffer, size_t len) {
        if (len < 3) return; // Geçersiz mouse raporu

        // Standart USB Mouse Boot Protocol:
        // Byte 0: Buttons (0: Left, 1: Right, 2: Middle)
        // Byte 1: X Displacement (Signed 8-bit)
        // Byte 2: Y Displacement (Signed 8-bit)

        state.leftButton = (buffer[0] & 0x01);
        state.rightButton = (buffer[0] & 0x02);
        
        int8_t dx = (int8_t)buffer[1];
        int8_t dy = (int8_t)buffer[2];

        // Pineapple DE koordinatlarını güncelle
        mouse_x += dx;
        mouse_y += dy;

        // DEBUG: Koordinatları terminale bas (test için)
        // printk("Mouse Moved: %d, %d\n", mouse_x, mouse_y);
    }
}