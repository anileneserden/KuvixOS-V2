#include <cpp/drivers/usb/hid/HIDManager.hpp>
#include <kernel/printk.h>

namespace Kuvix::Drivers::USB::HID {

    HIDManager& HIDManager::getInstance() {
        static HIDManager instance;
        return instance;
    }

    void HIDManager::registerDevice(HIDDevice* device) {
        currentDevice = device;
        printk("[HID Manager] New device registered: %s\n", device->getDeviceName());
    }

    void HIDManager::onReportReceived(uint8_t* buffer, size_t len) {
        if (currentDevice) {
            currentDevice->handleReport(buffer, len);
        }
    }
}

// xhci.c içinden çağrılacak olan C köprüsü
extern "C" void usb_hid_cpp_on_report(uint8_t* buf, size_t len) {
    Kuvix::Drivers::USB::HID::HIDManager::getInstance().onReportReceived(buf, len);
}