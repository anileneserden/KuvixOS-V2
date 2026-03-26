#pragma once
#include <stdint.h>

namespace kui {

struct MouseState {
    int x;
    int y;
    uint8_t buttons;
};

class Mouse {
public:
    static MouseState state();
};

} // namespace kui