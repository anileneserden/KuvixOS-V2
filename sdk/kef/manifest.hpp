#pragma once

#include <sdk/kef/app.hpp>

namespace kef {

struct WindowDefaults {
    int width;
    int height;
    const char* title;
};

struct AppManifest {
    AppKind kind;
    WindowDefaults window;
};

inline WindowDefaults DefaultWindowDefaults() {
    WindowDefaults defaults;
    defaults.width = 640;
    defaults.height = 480;
    defaults.title = "Kef Window";
    return defaults;
}

template <typename T>
AppManifest BuildManifest() {
    AppManifest manifest;
    T probe;
    manifest.kind = probe.Kind();
    manifest.window = DefaultWindowDefaults();
    return manifest;
}

} // namespace kef