#include "modapi/core/ModAPI.h"
#include "modapi/Version.h"
#include "modapi/block/BlockRegistry.h"
#include "modapi/core/FileUtils.h"
#include "modapi/core/Gloabl.h"
#include "modapi/core/RandomColorLogFormatter.h"
#include "modapi/gamerule/GameRuleRegistry.h"
#include "modapi/item/CreativeItemRegistry.h"
#include "modapi/item/ItemRegistry.h"
#include "modapi/loot_table/LootTableRegistry.h"
#include "modapi/recipe/RecipeRegistry.h"
#include "modapi/worldgen/FeatureRegistry.h"
#include <ll/api/event/EventBus.h>
#include <ll/api/io/Formatter.h>
#include <ll/api/mod/RegisterHelper.h>
#include <ll/api/reflection/Serialization.h>
#include <ll/api/service/GamingStatus.h>
#include <ll/api/utils/RandomUtils.h>
#include <nlohmann/json.hpp>
#include <ranges>

namespace modapi {

namespace {
// Set from `ModAPI::disable()`: the process is on its way out.
std::atomic_bool gShuttingDown = false;
} // namespace

// Declared in `modapi/Registry.h`, i.e. in the enclosing namespace and not in the inline one.
MOD_API bool isShuttingDown() { return gShuttingDown.load(); }

} // namespace modapi

namespace modapi::inline core {

void _setShuttingDown() { modapi::gShuttingDown.store(true); }

ModAPI& ModAPI::getInstance() {
    static ModAPI instance;
    return instance;
}

// The registry ready events are owned by ModAPI and have to be registered before a consumer can
// listen to them. This cannot happen from a static initialiser (the mod manager has not published
// ModAPI's own mod instance yet), so it happens here; `DeferredRegister` re-tries it anyway.
static void registerRegistryEvents() {
    modapi::item::ItemRegistry::ensureEventRegistered();
    modapi::item::CreativeItemRegistry::ensureEventRegistered();
    modapi::recipe::RecipeRegistry::ensureEventRegistered();
    modapi::gamerule::GameRuleRegistry::ensureEventRegistered();
    modapi::worldgen::FeatureRegistry::ensureEventRegistered();
    modapi::loot_table::LootTableRegistry::ensureEventRegistered();
    modapi::block::BlockRegistry::ensureEventRegistered();
}

bool ModAPI::load() {
    if (ll::getGamingStatus() == ll::GamingStatus::Running) {
        getLogger().error("It is prohibited to load ModAPI mod when the server is running.");
        return false;
    }
    getLogger().setFormatter(
        ll::makePolymorphic<RandomColorLogFormatter>(
            "{3:.3%T.} {2} {1} {0}",
            ll::io::Formatter::supportColorLog(),
            0b0010
        )
    );
    correctManifest();
    printLogo();
    registerRegistryEvents();
    return true;
}

bool ModAPI::enable() { return true; }

bool ModAPI::disable() {
    if (ll::getGamingStatus() != ll::GamingStatus::Stopping) {
        getLogger().error("It is prohibited to disable ModAPI mod when the server is not stopped.");
        return false;
    }
    // From here on the process is torn down and every global destructor has to stop touching
    // LeviLamina.
    _setShuttingDown();

    // LeviLamina keeps whatever this installs: a formatter (and any other object) whose code lives in
    // this DLL must not outlive it, or the engine calls into an unloaded module while it shuts down.
    // Putting the stock formatter back releases ours while both modules are still loaded.
    getLogger().setFormatter(
        ll::makePolymorphic<ll::io::PatternFormatter>(
            "{3:.3%T.} {2} {1} {0}",
            ll::io::Formatter::supportColorLog(),
            0b0010
        )
    );
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
    if (!core::writeFile(
            getSelf().getModDir() / "manifest.json",
            ll::reflection::serialize<nlohmann::ordered_json>(manifest)->dump(4)
        )) {
        getLogger().error("Failed to write manifest.json");
    }
}

ModAPI&         getInstance() { return ModAPI::getInstance(); }
ll::mod::Mod&   getSelfMod() { return getInstance().getSelf(); }
ll::io::Logger& getLogger() { return getSelfMod().getLogger(); }

std::weak_ptr<ll::mod::Mod> getSelfModPtr() {
    // `NativeMod::current()` resolves the module that compiled this call, i.e. ModAPI.dll, so this
    // yields ModAPI's own mod - never the mod that happens to be calling us.
    auto mod = ll::mod::NativeMod::current();
    if (!mod || mod->getName() != getSelfMod().getName()) return {};
    return mod;
}

} // namespace modapi::inline core

LL_REGISTER_MOD(modapi::ModAPI, modapi::ModAPI::getInstance());