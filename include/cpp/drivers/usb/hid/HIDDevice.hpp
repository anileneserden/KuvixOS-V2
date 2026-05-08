#ifndef KUVIX_HID_DEVICE_HPP
#define KUVIX_HID_DEVICE_HPP

#include <stdint.h>
#include <stddef.h>

namespace Kuvix::Drivers::USB::HID {

    class HIDDevice {
    public:
        HIDDevice() = default;
        virtual ~HIDDevice() = default;

        // xHCI sürücüsünden gelen ham USB paketini (report) işler
        virtual void handleReport(uint8_t* buffer, size_t len) = 0;
        
        virtual const char* getDeviceName() const = 0;
    };

}

#endif