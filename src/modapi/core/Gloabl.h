#pragma once
#include <ll/api/mod/Mod.h>

namespace modapi::inline core {
class ModAPI;
ModAPI&         getInstance();
ll::mod::Mod&   getSelfMod();
ll::io::Logger& getLogger();
} // namespace modapi::inline core