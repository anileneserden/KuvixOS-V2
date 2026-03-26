#include <ui/kui/cpp/test_ui.hpp>
#include <ui/kui/cpp/fb.hpp>
#include <ui/kui/cpp/graphics.hpp>
#include <ui/kui/cpp/color.hpp>
#include <ui/kui/cpp/print.hpp>
#include <ui/kui/cpp/ui_root.hpp>
#include <ui/kui/cpp/widgets/rect_widget.hpp>
#include <ui/kui/cpp/widgets/label_widget.hpp>
#include <ui/kui/cpp/widgets/panel_widget.hpp>

namespace kui::test {

static kui::UIRoot* g_ui = 0;

static void copy_text(char* dst, const char* src, int max_len) {
    if (!dst || max_len <= 0) return;

    int i = 0;
    if (src) {
        for (; src[i] && i < max_len - 1; i++) {
            dst[i] = src[i];
        }
    }
    dst[i] = '\0';
}

void initTestUI() {
    print("KUI widget test init\n");

    if (g_ui) return;

    g_ui = new kui::UIRoot();

    // Root panel
    kui::PanelWidget* root = g_ui->root();
    root->x = 0;
    root->y = 0;
    root->width = 1024;
    root->height = 768;
    root->backgroundColor = kui::Color::Gray;
    root->radius = 0;

    // Beyaz kart
    kui::RectWidget* whiteCard = new kui::RectWidget();
    copy_text(whiteCard->id, "whiteCard", 64);
    whiteCard->x = 20;
    whiteCard->y = 20;
    whiteCard->width = 180;
    whiteCard->height = 90;
    whiteCard->radius = 12;
    whiteCard->color = kui::Color::White;

    // Kırmızı küçük kutu
    kui::RectWidget* redBox = new kui::RectWidget();
    copy_text(redBox->id, "redBox", 64);
    redBox->x = 30;
    redBox->y = 30;
    redBox->width = 24;
    redBox->height = 24;
    redBox->radius = 4;
    redBox->color = kui::Color::Red;

    // Başlık
    kui::LabelWidget* title = new kui::LabelWidget();
    copy_text(title->id, "title", 64);
    title->x = 20;
    title->y = 140;
    title->color = kui::Color::White;
    copy_text(title->text, "KUI Widget Test", 128);

    // Alt başlık
    kui::LabelWidget* sub = new kui::LabelWidget();
    copy_text(sub->id, "sub", 64);
    sub->x = 20;
    sub->y = 160;
    sub->color = kui::Color::Red;
    copy_text(sub->text, "Rect + Label + Panel", 128);

    root->addChild(whiteCard);
    root->addChild(redBox);
    root->addChild(title);
    root->addChild(sub);
}

void tickTestUI() {
    kui::Framebuffer fb;
    kui::Graphics gfx(fb);

    gfx.clear(kui::Color::Gray);

    if (g_ui) {
        g_ui->draw(gfx);
    }

    gfx.present();
}

}