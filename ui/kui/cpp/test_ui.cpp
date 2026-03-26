#include <ui/kui/cpp/test_ui.hpp>
#include <ui/kui/cpp/fb.hpp>
#include <ui/kui/cpp/graphics.hpp>
#include <ui/kui/cpp/color.hpp>
#include <ui/kui/cpp/print.hpp>

namespace kui::test {

void initTestUI() {
    print("KUI test init\n");
}

void tickTestUI() {
    kui::Framebuffer fb;
    kui::Graphics gfx(fb);

    gfx.clear(kui::Color::Gray);
    gfx.drawTextUtf8(20, 20, kui::Color::White, "ONLY THIS");

    // çok önemli
    gfx.present();
}

}