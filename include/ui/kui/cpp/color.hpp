#pragma once
#include <stdint.h>

namespace kui {
    struct Color {
        static constexpr uint32_t Black = 0x00000000;
        static constexpr uint32_t White = 0x00FFFFFF;
        static constexpr uint32_t Red   = 0x00FF0000;
        static constexpr uint32_t Green = 0x0000FF00;
        static constexpr uint32_t Blue  = 0x000000FF;
        static constexpr uint32_t Gray  = 0x00202020;
    };
}