#ifndef KUVIX_MOUSE_DEVICE_HPP
#define KUVIX_MOUSE_DEVICE_HPP

#include <cpp/drivers/usb/hid/HIDDevice.hpp>

namespace Kuvix::Drivers::USB::HID {

    class MouseDevice : public HIDDevice {
    public:
        MouseDevice();
        ~MouseDevice() override = default;

        void handleReport(uint8_t* buffer, size_t len) override;
        const char* getDeviceName() const override { return "Kuvix USB Mouse"; }

    private:
        // Farenin anlık durumu
        struct {
            bool leftButton;
            bool rightButton;
            bool middleButton;
            int8_t lastDeltaX;
            int8_t lastDeltaY;
        } state;

        void processMouseReport(uint8_t* buffer);
    };

}

#endif