#include <ui/kui/cpp/input/mouse.hpp>

extern "C" {
    int mouse_get_x(void);
    int mouse_get_y(void);
    unsigned char mouse_get_buttons(void);
}

namespace kui {

MouseState Mouse::state() {
    MouseState s{};
    s.x = mouse_get_x();
    s.y = mouse_get_y();
    s.buttons = mouse_get_buttons();
    return s;
}

} // namespace kui