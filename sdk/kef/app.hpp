#pragma once

namespace kef {

enum class AppKind {
    Console = 1,
    Window = 2
};

class App {
public:
    virtual ~App() {}

    virtual AppKind Kind() const = 0;

    virtual void OnCreate() {}
    virtual void OnDestroy() {}
    virtual void OnUpdate() {}
};

} // namespace kef