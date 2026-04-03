#pragma once

#include <sdk/kef/manifest.hpp>

namespace kef {

class App;

typedef App* (*AppFactoryFn)();

template <typename T>
App* CreateAppInstance() {
    return new T();
}

template <typename T>
AppManifest CreateAppManifest() {
    return BuildManifest<T>();
}

} // namespace kef

#define KEF_APP(AppType)                           \
    extern "C" ::kef::App* kef_create_app() {    \
        return ::kef::CreateAppInstance<AppType>(); \
    }                                              \
    extern "C" ::kef::AppManifest kef_create_manifest() { \
        return ::kef::CreateAppManifest<AppType>(); \
    }