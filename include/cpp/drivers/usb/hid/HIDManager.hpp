#ifndef KUVIX_HID_MANAGER_HPP
#define KUVIX_HID_MANAGER_HPP

#include <cpp/drivers/usb/hid/HIDDevice.hpp>

namespace Kuvix::Drivers::USB::HID {

    class HIDManager {
    public:
        static HIDManager& getInstance();

        // Yeni bir aygıt takıldığında xHCI burayı çağırır
        void registerDevice(HIDDevice* device);
        
        // xHCI'dan veri geldiğinde ilgili aygıta iletir
        void onReportReceived(uint8_t* buffer, size_t len);

    private:
        HIDManager() = default;
        HIDDevice* currentDevice = nullptr; // Şimdilik tek aygıt desteği
    };

}

// C kodundan (xhci.c) çağrılabilmesi için extern "C" köprüsü
extern "C" {
    void usb_hid_cpp_on_report(uint8_t* buf, size_t len);
}

#endif