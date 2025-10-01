#pragma once
#include <ll/api/mod/Mod.h>

namespace modapi {
class ModApi;
ModApi&         getInstance();
ll::mod::Mod&   getSelfMod();
ll::io::Logger& getLogger();
} // namespace modapi