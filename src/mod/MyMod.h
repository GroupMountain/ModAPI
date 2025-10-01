#pragma once
#include <ll/api/mod/NativeMod.h>

namespace my_mod {

class MyMod {

public:
    static MyMod& getInstance();

    MyMod() : mSelf(*ll::mod::NativeMod::current()) {}

    [[nodiscard]] ll::mod::NativeMod& getSelf() const { return mSelf; }

    bool load();
    bool enable();
    bool disable();
    // bool unload();

private:
    ll::mod::NativeMod& mSelf;
};

} // namespace my_mod
