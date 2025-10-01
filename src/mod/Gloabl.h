#pragma once
#include <ll/api/mod/Mod.h>

namespace my_mod {
class MyMod;
MyMod&          getInstance();
ll::mod::Mod&   getSelfMod();
ll::io::Logger& getLogger();
} // namespace my_mod