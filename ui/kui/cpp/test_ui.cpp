#include <ui/kui/cpp/test_ui.hpp>
#include <ui/kui/cpp/fb.hpp>
#include <ui/kui/cpp/graphics.hpp>

namespace kui {
namespace test {

void runTestUI() {
    kui::Framebuffer fb;
    kui::Graphics gfx(fb);

    gfx.clear(0x00202020);

    gfx.fillRect(20, 20, 120, 80, 0x00FFFFFF);
    gfx.fillRect(30, 30, 20, 20, 0x00FF0000);

    gfx.putPixel(10, 10, 0x00FFFFFF);
}

} // namespace test
} // namespace kui