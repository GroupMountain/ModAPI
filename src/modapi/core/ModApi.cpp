#include "modapi/core/ModApi.h"
#include "modapi/core/Gloabl.h"
#include <ll/api/mod/RegisterHelper.h>
#include <ll/api/service/GamingStatus.h>

namespace modapi {

ModApi& ModApi::getInstance() {
    static ModApi instance;
    return instance;
}

bool ModApi::load() {
    if (ll::getGamingStatus() == ll::GamingStatus::Running) {
        getLogger().error("It is prohibited to load ModApi mod when the server is running.");
        return false;
    }
    return true;
}

bool ModApi::enable() { return true; }

bool ModApi::disable() {
    if (ll::getGamingStatus() != ll::GamingStatus::Stopping) {
        getLogger().error("It is prohibited to disable ModApi mod when the server is not stopped.");
        return false;
    }
    return true;
}

ModApi&         getInstance() { return ModApi::getInstance(); }
ll::mod::Mod&   getSelfMod() { return getInstance().getSelf(); }
ll::io::Logger& getLogger() { return getSelfMod().getLogger(); }

} // namespace modapi

LL_REGISTER_MOD(modapi::ModApi, modapi::ModApi::getInstance());