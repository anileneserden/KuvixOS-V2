#include <sdk/kef/prelude.hpp>

class HelloWindow : public Window {
public:
    HelloWindow() : Window(800, 480, "Hello Window") {}

    void OnCreate() override {
        Invalidate();
    }

    void OnDraw() override {
        DrawText(24, 24, "Hello Window from KEF SDK");
        DrawText(24, 48, "Default lifecycle and title metadata will live in sdk-core.");
    }
};

KEF_APP(HelloWindow)