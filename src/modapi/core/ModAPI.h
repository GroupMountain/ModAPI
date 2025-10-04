#pragma once
#include <ll/api/mod/NativeMod.h>

namespace modapi::inline core {

class ModAPI {

public:
    static ModAPI& getInstance();

    ModAPI() : mSelf(*ll::mod::NativeMod::current()) {}

    [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }

    bool load();
    bool enable();
    bool disable();

    void printLogo();
    void correctManifest();

private:
    ll::mod::NativeMod& mSelf;
};

} // namespace modapi::inline core