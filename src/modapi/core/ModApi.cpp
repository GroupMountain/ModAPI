#include "modapi/core/ModAPI.h"
#include "modapi/Version.h"
#include "modapi/core/Gloabl.h"
#include "modapi/core/RandomColorLogFormatter.h"
#include <gmlib/gm/io/FileUtils.h>
#include <ll/api/io/Formatter.h>
#include <ll/api/mod/RegisterHelper.h>
#include <ll/api/reflection/Serialization.h>
#include <ll/api/service/GamingStatus.h>
#include <nlohmann/json.hpp>

namespace modapi::inline core {

ModAPI& ModAPI::getInstance() {
    static ModAPI instance;
    return instance;
}

bool ModAPI::load() {
    if (ll::getGamingStatus() == ll::GamingStatus::Running) {
        getLogger().error("It is prohibited to load ModAPI mod when the server is running.");
        return false;
    }
    getLogger().setFormatter(ll::makePolymorphic<RandomColorLogFormatter>(
        "{3:.3%T.} {2} {1} {0}",
        ll::io::Formatter::supportColorLog(),
        0b0010
    ));
    correctManifest();
    printLogo();
    return true;
}

bool ModAPI::enable() { return true; }

bool ModAPI::disable() {
    if (ll::getGamingStatus() != ll::GamingStatus::Stopping) {
        getLogger().error("It is prohibited to disable ModAPI mod when the server is not stopped.");
        return false;
    }
    return true;
}

void ModAPI::printLogo() {
    std::vector<std::string> output = {
        R"(  __  __               _                      _  )",
        R"( |  \/  |             | |     /\             (_) )",
        R"( | \  / |   ___     __| |    /  \     _ __    _  )",
        R"( | |\/| |  / _ \   / _` |   / /\ \   | '_ \  | | )",
        R"( | |  | | | (_) | | (_| |  / ____ \  | |_) | | | )",
        R"( |_|  |_|  \___/   \__,_| /_/    \_\ | .__/  |_| )",
        R"(                                     | |         )",
        R"(                                     |_|         )",
        R"(                                                 )",
        fmt::format("ModAPI v{0}", getSelf().getManifest().version->to_string()),
        fmt::format("Author: {0}", "GroupMountain")
    };
    auto center = std::ranges::max_element(output, {}, &std::string::size)->size();
    for (auto& line : output) {
        getLogger().info(fmt::format("{0:^{1}}", line, center));
    }
}

void ModAPI::correctManifest() {
    auto& manifest       = const_cast<ll::mod::Manifest&>(getSelf().getManifest());
    manifest.type        = "native";
    manifest.version     = ll::data::Version{MODAPI_FILE_VERSION_STRING};
    manifest.author      = "GroupMountain";
    manifest.description = "Group Mountain Mod API";
    manifest.extraInfo.reset();
    manifest.dependencies.reset();
    manifest.optionalDependencies.reset();
    manifest.conflicts.reset();
    manifest.loadBefore.reset();
    gmlib::file_utils::writeFile(
        getSelf().getModDir() / "manifest.json",
        ll::reflection::serialize<nlohmann::ordered_json>(manifest)->dump(4)
    );
}

ModAPI&         getInstance() { return ModAPI::getInstance(); }
ll::mod::Mod&   getSelfMod() { return getInstance().getSelf(); }
ll::io::Logger& getLogger() { return getSelfMod().getLogger(); }

} // namespace modapi::inline core

LL_REGISTER_MOD(modapi::ModAPI, modapi::ModAPI::getInstance());