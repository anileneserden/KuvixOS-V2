#include <ui/kui/cpp/test_ui_c_bridge.h>
#include <ui/kui/cpp/test_ui.hpp>

extern "C" void kui_cpp_test_ui_init(void) {
    kui::test::initTestUI();
}

extern "C" void kui_cpp_test_ui_tick(void) {
    kui::test::tickTestUI();
}