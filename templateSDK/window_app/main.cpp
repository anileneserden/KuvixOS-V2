#include <sdk/kef/prelude.hpp>

class TemplateWindowApp : public Window {
public:
    TemplateWindowApp() : Window(800, 480, "Template Window") {}

    void OnCreate() override {
        Invalidate();
    }

    void OnDraw() override {
        DrawText(24, 24, "Template window app");
        DrawText(24, 48, "Connect this class to a JSON layout and widget binding system.");
    }
};

KEF_APP(TemplateWindowApp)