#pragma once
#include <ll/api/mod/Mod.h>
#include <memory>

namespace modapi::inline core {
class ModAPI;
ModAPI&                     getInstance();
ll::mod::Mod&               getSelfMod();
ll::io::Logger&             getLogger();
std::weak_ptr<ll::mod::Mod> getSelfModPtr();

// Marks the process as shutting down (set from ModAPI::disable() / unload()).
void _setShuttingDown();
} // namespace modapi::inline core
