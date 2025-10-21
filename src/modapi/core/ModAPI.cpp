#include "modapi/core/ModAPI.h"
#include "modapi/Version.h"
#include "modapi/core/Gloabl.h"
#include "modapi/core/RandomColorLogFormatter.h"
#include <gmlib/gm/io/FileUtils.h>
#include <ll/api/io/Formatter.h>
#include <ll/api/mod/RegisterHelper.h>
#include <ll/api/reflection/Serialization.h>
#include <ll/api/service/GamingStatus.h>
#include <ll/api/utils/RandomUtils.h>
#include <nlohmann/json.hpp>
#include <ranges>

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
        R"(   __  __               _              _____    _____  )",
        R"(  |  \/  |             | |     /\     |  __ \  |_   _| )",
        R"(  | \  / |   ___     __| |    /  \    | |__) |   | |   )",
        R"(  | |\/| |  / _ \   / _` |   / /\ \   |  ___/    | |   )",
        R"(  | |  | | | (_) | | (_| |  / ____ \  | |       _| |_  )",
        R"(  |_|  |_|  \___/   \__,_| /_/    \_\ |_|      |_____| )",
        R"(                                                       )",
        R"(  -------------- Group Mountain Mod API -------------- )",
        R"(                                                       )",
        fmt::format("ModAPI v{0}", getSelf().getManifest().version->to_string()),
        fmt::format("Author: {0}", "GroupMountain")
    };
    // clang-format off
    auto center = std::ranges::max_element(output, {}, &std::string::size)->size();
    if (ll::random_utils::rand<uchar>(0, 10) <= 5) {
        getLogger().info(
            output
                | std::views::transform([&](auto&& line) {
                    return fmt::format("{0:^{1}}", line, center);
                })
                | std::views::join_with('\n')
                | std::ranges::to<std::string>()
        );
    } else {
        for (auto& line : output) {
            getLogger().info(fmt::format("{0:^{1}}", line, center));
        }
    }
    // clang-format on
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