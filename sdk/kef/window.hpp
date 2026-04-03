#pragma once

#include <sdk/kef/app.hpp>

namespace kef {

class Window : public App {
public:
    Window(int width = 640, int height = 480, const char* title = "Kef Window")
        : width_(width), height_(height), title_(title ? title : "Kef Window") {}

    AppKind Kind() const override {
        return AppKind::Window;
    }

    virtual void OnDraw() {}
    virtual void OnKey(int key) { (void)key; }
    virtual void OnMouse(int x, int y, int buttons) {
        (void)x;
        (void)y;
        (void)buttons;
    }

    int Width() const { return width_; }
    int Height() const { return height_; }
    const char* Title() const { return title_; }

protected:
    void DrawText(int x, int y, const char* text);
    void Close();
    void Invalidate();

private:
    int width_;
    int height_;
    const char* title_;
};

} // namespace kef