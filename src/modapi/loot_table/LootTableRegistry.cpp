#include "modapi/loot_table/LootTableRegistry.h"
#include "modapi/core/Gloabl.h"
#include "modapi/core/RegistryEvent.h"
#include "modapi/item/ItemRegistry.h"
#include <atomic>
#include <fstream>
#include <functional>
#include <ll/api/event/EventBus.h>
#include <ll/api/memory/Hook.h>
#include <mc/common/SharedConstants.h>
#include <mc/deps/core/sem_ver/SemVersion.h>
#include <mc/deps/json/Reader.h>
#include <mc/deps/json/Value.h>
#include <mc/world/level/storage/loot/LootPool.h>
#include <mc/world/level/storage/loot/LootPoolTiers.h>
#include <mc/world/level/storage/loot/LootTable.h>
#include <mc/world/level/storage/loot/LootTables.h>
#include <mc/world/level/storage/loot/RandomValueBounds.h>
#include <mc/world/level/storage/loot/entries/LootItem.h>
#include <mc/world/level/storage/loot/entries/LootPoolEntry.h>
#include <mc/world/level/storage/loot/entries/LootTableReference.h>
#include <mc/world/level/storage/loot/functions/LootItemFunction.h>
#include <mc/world/level/storage/loot/functions/LootItemFunctions.h>
#include <mc/world/level/storage/loot/predicates/LootItemCondition.h>
#include <nlohmann/json.hpp>
#include <sstream>
#include <unordered_map>

namespace modapi::inline loot_table {

namespace {

// `LootTable::deserialize` reads the document against a format version. It is taken from the engine's
// own constant rather than parsed from a string: `SemVersion` holds a packed pointer, and a parsed one
// is owned - its destructor (and the copy on return) then freed a pointer that was not theirs, which
// crashed inside `mi_free`. The constant's storage is static, so the version stays valid and safe to
// destroy, and it is the version this build writes its own documents for.
::SemVersion currentJsonVersion() { return ::SemVersion{::SharedConstants::CurrentGameSemVersion()}; }

// `nlohmann` is what reads the JSON files, but the engine deserializes through its own DOM.
::Json::Value toJsonValue(::nlohmann::json const& json) {
    static ::Json::Reader reader;
    ::Json::Value         value;
    reader.parse(json.dump(), value, true);
    return value;
}

// "loot_tables/chests/simple_dungeon.json" is the form `LootTables` uses as its key.
std::string directoryFromPath(std::filesystem::path const& jsonPath) {
    auto const normalized = jsonPath.generic_string();
    auto const at         = normalized.rfind("loot_tables/");
    if (at != std::string::npos) return normalized.substr(at);

    std::string name = jsonPath.filename().generic_string();
    if (name.empty()) return {};
    return "loot_tables/" + name;
}

// `RandomValueBounds` is two floats that LeviLamina emits as unnamed storage, hence `.as<float>()` here;
// `src/mc/RandomValueBounds.cpp` defines the constructors the mod prelink does not provide.
void setBounds(::RandomValueBounds& bounds, float min, float max) {
    bounds.mUnk98064d.as<float>() = min;
    bounds.mUnk79891e.as<float>() = max;
}

} // namespace

// `LootTables::lookupByName` is the single place vanilla resolves a loot table - resource pack
// loading, chest contents, entity drops and `LootTableReference` entries all go through it.
LL_TYPE_INSTANCE_HOOK(
    LootTablesLookupHook,
    HookPriority::Normal,
    ::LootTables,
    &::LootTables::lookupByName,
    ::LootTable*,
    ::std::string const&   dir,
    ::ResourcePackManager& resourceLoader
) {
    // Bind first: the ready event replays the deferred registrations, and the table that was just
    // registered has to be what this very lookup returns.
    auto& registry = LootTableRegistry::getInstance();
    registry._bindRegistry(*this, resourceLoader);
    if (auto* ours = registry._findTable(dir)) return ours;
    return origin(dir, resourceLoader);
}

struct LootTableRegistry::Impl {
    ll::memory::HookRegistrar<LootTablesLookupHook> mHook;
    // The instance vanilla resolves tables through (and the resource pack manager it hands the
    // engine's own loader), remembered by the hook above.
    ::LootTables*          mTables              = nullptr;
    ::ResourcePackManager* mResourcePackManager = nullptr;
    bool                   mReady               = false;

    // The tables this registry owns, keyed by the directory vanilla looks them up by. The engine's
    // own cache is left alone: it is the engine's bookkeeping, and writing into it from a mod is what
    // made the creative item registry crash (see the changelog).
    std::unordered_map<std::string, std::unique_ptr<::LootTable>> mOwnedTables;
};

LootTableRegistry::LootTableRegistry() : pImpl(std::make_unique<Impl>()) {}

LootTableRegistry& LootTableRegistry::getInstance() {
    // Deliberately leaked: the hook registrar this instance owns must not unregister its hooks
    // while the process/DLL is being torn down.
    static LootTableRegistry& instance = *new LootTableRegistry();
    return instance;
}

bool LootTableRegistry::isReady() const noexcept { return pImpl->mReady; }

std::unique_ptr<::LootTable> LootTableRegistry::buildTable(std::string const& tableDir, std::string const& rawJson) {
    auto json  = ::nlohmann::json::parse(rawJson, nullptr, true, true);
    auto table = std::make_unique<::LootTable>();
    table->deserialize(toJsonValue(json), true, currentJsonVersion());
    *table->mDir = tableDir;
    return table;
}


void LootTableRegistry::ensureEventRegistered() {
    // The vanilla hook lives in the registry's implementation, so the singleton has to exist before
    // the first lookup; every consumer goes through this entry point.
    (void)getInstance();
    static std::atomic_bool registered = false;
    core::ensureRegistryEventRegistered<EventType>(registered);
}

LootTableRegistry::ProductRef
LootTableRegistry::registerLootTableFromMemoryJson(std::string const& tableDir, std::string const& rawJson) {
    if (tableDir.empty()) {
        core::getLogger().error("LootTableRegistry: a loot table needs the directory vanilla looks it up by.");
        return {};
    }
    if (!pImpl->mReady) {
        core::getLogger().error(
            "LootTableRegistry: loot tables can only be registered once the engine looked one up; "
            "use DeferredRegister<LootTableRegistry, ...> to register tables before the server starts."
        );
        return {};
    }
    try {
        // The engine's own deserializer builds the table (`buildTable`), so every entry, condition and
        // function the game supports works and no loot logic is reimplemented here.
        return registerLootTable(tableDir, buildTable(tableDir, rawJson));
    } catch (std::exception const& e) {
        core::getLogger().error("LootTableRegistry: could not register the loot table '{}': {}", tableDir, e.what());
        return {};
    } catch (...) {
        core::getLogger().error("LootTableRegistry: could not register the loot table '{}'.", tableDir);
        return {};
    }
}

LootTableRegistry::ProductRef LootTableRegistry::registerLootTableFromJsonFile(std::filesystem::path const& jsonPath) {
    auto const tableDir = directoryFromPath(jsonPath);
    if (tableDir.empty()) {
        core::getLogger().error("LootTableRegistry: '{}' is not a loot table path.", jsonPath.string());
        return {};
    }
    try {
        std::ifstream file(jsonPath, std::ios::binary);
        if (!file) {
            core::getLogger().error("LootTableRegistry: could not open '{}'.", jsonPath.string());
            return {};
        }
        std::ostringstream buffer;
        buffer << file.rdbuf();
        return registerLootTableFromMemoryJson(tableDir, buffer.str());
    } catch (...) {
        core::getLogger().error("LootTableRegistry: could not read '{}'.", jsonPath.string());
        return {};
    }
}

LootTableRegistry::ProductRef
LootTableRegistry::registerLootTableFromJsonValue(std::string const& tableDir, ::Json::Value const& table) {
    if (tableDir.empty()) {
        core::getLogger().error("LootTableRegistry: a loot table needs the directory vanilla looks it up by.");
        return {};
    }
    if (!pImpl->mReady) {
        core::getLogger().error(
            "LootTableRegistry: loot tables can only be registered once the engine looked one up; "
            "use DeferredRegister<LootTableRegistry, ...> to register tables before the server starts."
        );
        return {};
    }
    try {
        auto built = std::make_unique<::LootTable>();
        built->deserialize(table, true, currentJsonVersion());
        return registerLootTable(tableDir, std::move(built));
    } catch (std::exception const& e) {
        core::getLogger().error("LootTableRegistry: could not deserialize the loot table '{}': {}", tableDir, e.what());
        return {};
    } catch (...) {
        core::getLogger().error("LootTableRegistry: could not deserialize the loot table '{}'.", tableDir);
        return {};
    }
}

LootTableRegistry::ProductRef
LootTableRegistry::registerLootTable(std::string const& tableDir, std::unique_ptr<::LootTable> table) {
    if (tableDir.empty() || !table) {
        core::getLogger().error("LootTableRegistry: a loot table needs a directory and a table.");
        return {};
    }
    try {
        // `getDir` is what the engine reports for a table; keep it in sync with the key it is found by.
        *table->mDir = tableDir;
        auto& slot   = pImpl->mOwnedTables[tableDir];
        slot         = std::move(table);
        auto* result = slot.get();
        core::getLogger().info("LootTableRegistry: registered the loot table '{}'.", tableDir);
        return result;
    } catch (std::exception const& e) {
        core::getLogger().error("LootTableRegistry: could not register the loot table '{}': {}", tableDir, e.what());
        return {};
    } catch (...) {
        core::getLogger().error("LootTableRegistry: could not register the loot table '{}'.", tableDir);
        return {};
    }
}

LootTableRegistry::ProductRef
LootTableRegistry::registerLootTable(std::string const& tableDir, LootTableBuilder& builder) {
    return registerLootTable(tableDir, builder._build());
}

LootTableRegistry::ProductRef
LootTableRegistry::_registerTable(std::function<std::unique_ptr<ICustomLootTable>()>&& factory) {
    if (!pImpl->mReady) {
        core::getLogger().error(
            "LootTableRegistry: loot tables can only be registered once the engine looked one up; "
            "use DeferredRegister<LootTableRegistry, ...> to register tables before the server starts."
        );
        return {};
    }
    try {
        auto entry = factory();
        if (!entry) return {};
        // Same contract as the other entry types: `_init()` does the engine work, and what it produces
        // is what gets registered (the entry object itself is dropped).
        auto table = entry->_init();
        if (!table) {
            core::getLogger().error("LootTableRegistry: the loot table entry built no table.");
            return {};
        }
        auto const dir = *table->mDir;
        if (dir.empty()) {
            core::getLogger().error(
                "LootTableRegistry: the loot table entry left no directory on its table; use "
                "LootTableRegistry::buildTable or registerLootTable to set one."
            );
            return {};
        }
        return registerLootTable(dir, std::move(table));
    } catch (std::exception const& e) {
        core::getLogger().error("LootTableRegistry: could not build a loot table entry: {}", e.what());
        return {};
    } catch (...) {
        core::getLogger().error("LootTableRegistry: could not build a loot table entry.");
        return {};
    }
}

bool LootTableRegistry::unregisterLootTable(std::string const& tableDir) {
    // Only the table this registry owns is dropped. `LootTables::mLootTables` is the engine's own cache
    // and is left alone: it never holds our tables (the hook answers before `origin` runs), and erasing
    // a cached vanilla table pulls it out from under whoever is still holding the pointer - the engine
    // resolves references lazily, so that pointer can be in use.
    return pImpl->mOwnedTables.erase(tableDir) > 0;
}

LootTableRegistry::Product* LootTableRegistry::_findTable(std::string const& tableDir) {
    auto it = pImpl->mOwnedTables.find(tableDir);
    return it != pImpl->mOwnedTables.end() ? it->second.get() : nullptr;
}

LootTableRegistry::ProductRef LootTableRegistry::getLootTable(std::string const& tableDir) {
    if (auto* ours = _findTable(tableDir)) return ours;
    // Vanilla's own tables are resolved by the engine, which needs the instance and a resource pack
    // manager - both only known once a lookup happened.
    if (pImpl->mTables == nullptr || pImpl->mResourcePackManager == nullptr) return {};
    try {
        return pImpl->mTables->lookupByName(tableDir, *pImpl->mResourcePackManager);
    } catch (...) {
        return {};
    }
}

void LootTableRegistry::_bindRegistry(::LootTables& tables, ::ResourcePackManager& resourcePackManager) {
    pImpl->mTables              = &tables;
    pImpl->mResourcePackManager = &resourcePackManager;
    if (pImpl->mReady) return;

    pImpl->mReady = true;
    ll::event::EventBus::getInstance().publish(LootTableReadyEvent{*this});
}

LootTableRegistry& LootTableReadyEvent::registry() const { return mRegistry; }

// ---------------------------------------------------------------------------------------------
// LootTableBuilder
// ---------------------------------------------------------------------------------------------

struct LootTableBuilder::Impl {
    std::vector<std::unique_ptr<::LootPool>> mPools;
};

LootTableBuilder::LootTableBuilder() : pImpl(std::make_unique<Impl>()) {}
LootTableBuilder::~LootTableBuilder()                                      = default;
LootTableBuilder::LootTableBuilder(LootTableBuilder&&) noexcept            = default;
LootTableBuilder& LootTableBuilder::operator=(LootTableBuilder&&) noexcept = default;

bool LootTableBuilder::empty() const { return pImpl->mPools.empty(); }

size_t LootTableBuilder::poolCount() const { return pImpl->mPools.size(); }

LootTableBuilder&
LootTableBuilder::addItemPool(std::string itemName, int weight, int quality, int minCount, int maxCount) {
    auto item = ::modapi::item::ItemRegistry::getInstance().getItem(itemName);
    if (!item) {
        core::getLogger().error("LootTableBuilder: '{}' is not a registered item.", itemName);
        return *this;
    }
    try {
        auto entry                = std::make_unique<::LootItem>();
        entry->mWeight            = weight;
        entry->mQuality           = quality;
        entry->mItem              = item.get();
        *entry->mOriginalItemName = itemName;
        // A loot item has no count of its own: in BDS the count is the `set_count` function, which the
        // engine builds from its own JSON - ask it for one, so the counts are the engine's.
        if (minCount != 1 || maxCount != 1) {
            auto count = ::nlohmann::json::array({
                {{"function", "set_count"}, {"count", {{"min", minCount}, {"max", maxCount}}}}
            });
            for (auto& function : ::LootItemFunctions::deserialize(toJsonValue(count), true)) {
                entry->mFunctions->push_back(std::move(function));
            }
        }

        auto pool = std::make_unique<::LootPool>();
        setBounds(*pool->mRolls, 1.0f, 1.0f);
        setBounds(*pool->mBonusRolls, 0.0f, 0.0f);
        pool->mEntries->push_back(std::move(entry));
        pImpl->mPools.push_back(std::move(pool));
    } catch (std::exception const& e) {
        core::getLogger().error("LootTableBuilder: could not add an item pool for '{}': {}", itemName, e.what());
    } catch (...) {
        core::getLogger().error("LootTableBuilder: could not add an item pool for '{}'.", itemName);
    }
    return *this;
}

LootTableBuilder& LootTableBuilder::addReferencePool(std::string tableDir, int weight, int quality) {
    try {
        auto entry      = std::make_unique<::LootTableReference>();
        entry->mWeight  = weight;
        entry->mQuality = quality;
        *entry->mDir    = std::move(tableDir);

        auto pool = std::make_unique<::LootPool>();
        setBounds(*pool->mRolls, 1.0f, 1.0f);
        setBounds(*pool->mBonusRolls, 0.0f, 0.0f);
        pool->mEntries->push_back(std::move(entry));
        pImpl->mPools.push_back(std::move(pool));
    } catch (std::exception const& e) {
        core::getLogger().error("LootTableBuilder: could not add a reference pool: {}", e.what());
    } catch (...) {
        core::getLogger().error("LootTableBuilder: could not add a reference pool.");
    }
    return *this;
}

LootTableBuilder&
LootTableBuilder::addEntry(std::unique_ptr<::LootPoolEntry> entry, int weight, int quality, float rolls) {
    if (!entry) {
        core::getLogger().error("LootTableBuilder: an entry is required.");
        return *this;
    }
    try {
        entry->mWeight  = weight;
        entry->mQuality = quality;

        auto pool = std::make_unique<::LootPool>();
        setBounds(*pool->mRolls, rolls, rolls);
        setBounds(*pool->mBonusRolls, 0.0f, 0.0f);
        pool->mEntries->push_back(std::move(entry));
        pImpl->mPools.push_back(std::move(pool));
    } catch (std::exception const& e) {
        core::getLogger().error("LootTableBuilder: could not add a custom entry: {}", e.what());
    } catch (...) {
        core::getLogger().error("LootTableBuilder: could not add a custom entry.");
    }
    return *this;
}

std::unique_ptr<::LootTable> LootTableBuilder::_build() {
    auto table = std::make_unique<::LootTable>();
    for (auto& pool : pImpl->mPools) {
        // The pools are handed over, so a builder is registered once.
        table->mPools->push_back(std::move(pool));
    }
    pImpl->mPools.clear();
    return table;
}
} // namespace modapi::inline loot_table
