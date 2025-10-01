#include "mod/MyMod.h"
#include "mod/Gloabl.h"
#include <ll/api/mod/RegisterHelper.h>

namespace my_mod {

MyMod& MyMod::getInstance() {
    static MyMod instance;
    return instance;
}

bool MyMod::load() { return true; }

bool MyMod::enable() { return true; }

bool MyMod::disable() { return true; }

// bool MyMod::unload() { return true; }

MyMod&          getInstance() { return MyMod::getInstance(); }
ll::mod::Mod&   getSelfMod() { return getInstance().getSelf(); }
ll::io::Logger& getLogger() { return getSelfMod().getLogger(); }

} // namespace my_mod

LL_REGISTER_MOD(my_mod::MyMod, my_mod::MyMod::getInstance());