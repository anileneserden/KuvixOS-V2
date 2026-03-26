#include <ui/kui/cpp/test_ui.hpp>
#include <ui/kui/cpp/fb.hpp>
#include <ui/kui/cpp/graphics.hpp>
#include <ui/kui/cpp/color.hpp>

namespace kui::test {

    void runTestUI() {
        kui::Framebuffer fb;
        kui::Graphics gfx(fb);

        gfx.clear(kui::Color::Gray);

        gfx.fillRect(20, 20, 120, 80, kui::Color::White);
        gfx.fillRect(30, 30, 20, 20, kui::Color::Red);

        gfx.drawTextUtf8(20, 120, kui::Color::White, "Deneme");
        gfx.drawTextUtf8(20, 140, kui::Color::Red, "KUI C++ Test");
    }

}