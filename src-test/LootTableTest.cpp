#include "Global.h"
#include "TestReport.hpp"
#include "modapi/DeferredRegister.h"
#include "modapi/loot_table/LootTableRegistry.h"
#include <filesystem>
#include <mc/deps/core/math/Vec2.h>
#include <mc/deps/core/math/Vec3.h>
#include <mc/server/SimulatedPlayer.h>
#include <mc/util/LootTableUtils.h>
#include <mc/world/item/ItemStack.h>
#include <mc/world/level/dimension/DimensionType.h>
#include <ll/api/service/Bedrock.h>
#include <mc/deps/json/Reader.h>
#include <mc/deps/json/Value.h>
#include <mc/world/level/Level.h>
#include <mc/world/level/storage/loot/LootTable.h>
#include <mc/world/level/storage/loot/LootTables.h>
#include <mc/world/level/storage/loot/entries/LootItem.h>
#include <mc/world/level/storage/loot/entries/LootTableReference.h>
#include <mc/world/level/storage/loot/LootPool.h>

namespace {

constexpr std::string_view TestJsonDir  = "loot_tables/modapi_test/memory.json";
constexpr std::string_view TestBuiltDir = "loot_tables/modapi_test/builder.json";
constexpr std::string_view TestEntryDir = "loot_tables/modapi_test/entry.json";
constexpr std::string_view TestLoadDir  = "loot_tables/modapi_test/from_load.json";
constexpr std::string_view TestGenerateDir = "loot_tables/modapi_test/generate.json";
constexpr std::string_view VanillaChest   = "loot_tables/chests/simple_dungeon.json";
constexpr std::string_view UnknownDir   = "loot_tables/modapi_test/definitely_missing.json";

constexpr std::string_view DiamondTable =
    R"({"pools":[{"rolls":1,"entries":[{"type":"item","name":"minecraft:diamond","weight":1}]}]})";

// Registered while the mod loads, i.e. before the engine has looked a single table up. The engine's
// `LootTable::deserialize` resolves items, so it cannot run yet: the call is rejected and the mod is
// pointed at `DeferredRegister`, which is what the `deferred` section below checks.
bool gLoadRejected = false;

void registerAtLoad() {
    auto& registry = modapi::LootTableRegistry::getInstance();
    registry.ensureEventRegistered();
    test::getLogger().info(
        "[LOOTTABLE] (the next ERR line is expected: registering a loot table before the engine looked one up)"
    );
    gLoadRejected =
        !registry.registerLootTableFromMemoryJson(std::string{TestLoadDir}, std::string{DiamondTable}).has_value();
}

// Registered as soon as the engine first looks a loot table up: `DeferredRegister` waits for
// `LootTableReadyEvent`, which is the first moment `_init()` may run the engine's deserializer.
class TestLootTable : public modapi::loot_table::ICustomLootTable {
public:
    std::unique_ptr<::LootTable> _init() override {
        return modapi::LootTableRegistry::buildTable(std::string{TestEntryDir}, std::string{DiamondTable});
    }
};

modapi::DeferredRegister<modapi::LootTableRegistry, TestLootTable> gLootTable{};

// The engine's JSON parser only knows its built-in entry types (their dispatcher is internal to the
// server build), so an entry with behaviour of its own is handed over in code instead.
class TestLootEntry : public ::LootPoolEntry {
public:
    bool _createItem(::std::vector<::ItemStack>& output, ::Random& random, ::LootTableContext& context)
        const override {
        (void)output;
        (void)random;
        (void)context;
        return true;
    }

    ::LootPoolEntry::EntryType getEntryType() const override { return ::LootPoolEntry::EntryType::LootItem; }
};

::Json::Value parseJson(std::string const& text) {
    static ::Json::Reader reader;
    ::Json::Value         value;
    reader.parse(text, value, true);
    return value;
}

} // namespace

int runLootTableTests() {
    test::Report report{"[LOOTTABLE]", "loottable-test-report.txt"};

    auto& registry = modapi::LootTableRegistry::getInstance();

    // Everything below needs the engine to have resolved a table at least once: that is what applies
    // the queued registrations and publishes `LootTableReadyEvent`.
    report.section("bind", [&] {
        report.isTrue("loadTimeRejected", gLoadRejected);

        auto level = ll::service::getLevel();
        if (!level) {
            report.skip("level");
            return;
        }
        auto* resourcePacks = level->getServerResourcePackManager();
        if (resourcePacks == nullptr) {
            report.skip("resourcePackManager");
            return;
        }
        auto& tables = level->getLootTables();

        auto* vanilla = tables.lookupByName(std::string{VanillaChest}, *resourcePacks);
        report.isTrue("vanillaResolves", vanilla != nullptr);
        report.isTrue("isReady", registry.isReady());

        // The load time call was rejected, so there is no such table: registering that early is what
        // `DeferredRegister` is for (checked in the `deferred` section).
        report.isTrue("loadTimeTableAbsent", registry._findTable(std::string{TestLoadDir}) == nullptr);
    });

    report.section("json.text", [&] {
        auto table = registry.registerLootTableFromMemoryJson(std::string{TestJsonDir}, std::string{DiamondTable});
        report.isTrue("registered", table.has_value());
        if (table.has_value()) {
            report.isTrue("onePool", table->mPools->size() == 1);
            report.isTrue("oneEntry", table->mPools->at(0)->mEntries->size() == 1);
            report.isTrue(
                "entryIsItem",
                table->mPools->at(0)->mEntries->at(0)->getEntryType() == ::LootPoolEntry::EntryType::LootItem
            );
        } else {
            report.skip("onePool");
            report.skip("oneEntry");
            report.skip("entryIsItem");
        }
        report.isTrue("findable", registry._findTable(std::string{TestJsonDir}) == table.as_ptr());
    });

    report.section("json.value", [&] {
        auto const dir   = std::string{"loot_tables/modapi_test/value.json"};
        auto       table = registry.registerLootTableFromJsonValue(dir, parseJson(std::string{DiamondTable}));
        report.isTrue("registered", table.has_value());
        report.isTrue("findable", registry._findTable(dir) == table.as_ptr());
    });

    report.section("builder", [&] {
        modapi::loot_table::LootTableBuilder builder;
        builder.addItemPool("minecraft:emerald", 3, 0, 2, 5);
        builder.addReferencePool(std::string{TestJsonDir});
        report.isTrue("poolCount", builder.poolCount() == 2);
        report.isTrue("notEmpty", !builder.empty());

        auto table = registry.registerLootTable(std::string{TestBuiltDir}, builder);
        report.isTrue("registered", table.has_value());
        report.isTrue("emptyAfterRegister", builder.empty());
        if (table.has_value()) {
            report.isTrue("twoPools", table->mPools->size() == 2);
            report.isTrue(
                "firstIsItem",
                table->mPools->at(0)->mEntries->at(0)->getEntryType() == ::LootPoolEntry::EntryType::LootItem
            );
            report.isTrue(
                "secondIsReference",
                table->mPools->at(1)->mEntries->at(0)->getEntryType()
                    == ::LootPoolEntry::EntryType::LootTableReference
            );
            auto* item = static_cast<::LootItem*>(table->mPools->at(0)->mEntries->at(0).get());
            report.isTrue("itemCount", item->mFunctions->size() == 1);
        // A custom entry: what the JSON `"type"` cannot express, handed over in code.
        modapi::loot_table::LootTableBuilder custom;
        auto                               customEntry = std::make_unique<TestLootEntry>();
        auto*                              rawEntry    = customEntry.get();
        custom.addEntry(std::move(customEntry), 2, 7, 3.0f);
        report.isTrue("custom.poolCount", custom.poolCount() == 1);

        auto customTable = registry.registerLootTable(std::string{"loot_tables/modapi_test/custom.json"}, custom);
        report.isTrue("custom.registered", customTable.has_value());
        report.isTrue(
            "custom.entryIsOurs",
            customTable.has_value() && customTable->mPools->size() == 1
                && customTable->mPools->at(0)->mEntries->at(0).get() == rawEntry
        );
        } else {
            report.skip("twoPools");
            report.skip("firstIsItem");
            report.skip("secondIsReference");
            report.skip("itemCount");
        }
    });

    report.section("engine", [&] {
        // What the hook is for: the engine resolving a directory ends up with the table this registry
        // built, not with a resource pack file.
        auto level = ll::service::getLevel();
        if (!level) {
            report.skip("level");
            return;
        }
        auto* resourcePacks = level->getServerResourcePackManager();
        if (resourcePacks == nullptr) {
            report.skip("resourcePackManager");
            return;
        }
        auto& tables = level->getLootTables();

        auto* registered = registry._findTable(std::string{TestJsonDir});
        report.isTrue("registeredTableExists", registered != nullptr);
        report.isTrue(
            "servesRegistered",
            registered != nullptr && tables.lookupByName(std::string{TestJsonDir}, *resourcePacks) == registered
        );

        auto* built = registry._findTable(std::string{TestBuiltDir});
        report.isTrue("builtTableExists", built != nullptr);
        report.isTrue(
            "servesBuilt",
            built != nullptr && tables.lookupByName(std::string{TestBuiltDir}, *resourcePacks) == built
        );

        auto* deferred = registry._findTable(std::string{TestEntryDir});
        report.isTrue("deferredTableExists", deferred != nullptr);
        report.isTrue(
            "servesDeferred",
            deferred != nullptr && tables.lookupByName(std::string{TestEntryDir}, *resourcePacks) == deferred
        );

        report.isTrue("getLootTableVanilla", registry.getLootTable(std::string{VanillaChest}).has_value());
    });

    report.section("deferred", [&] {
        // Registered by the first lookup, through `LootTableReadyEvent`.
        report.isTrue("registered", gLootTable.isRegistered());
        report.isTrue("product", gLootTable.get().has_value());
        if (gLootTable.get().has_value()) {
            report.isTrue("findable", registry._findTable(std::string{TestEntryDir}) == gLootTable.get().as_ptr());
        } else {
            report.skip("findable");
        }
    });

    report.section("override", [&] {
        // A table registered under an existing directory wins over the resource pack one, and giving it
        // up hands the directory back to the engine.
        auto level = ll::service::getLevel();
        if (!level) {
            report.skip("level");
            return;
        }
        auto* resourcePacks = level->getServerResourcePackManager();
        if (resourcePacks == nullptr) {
            report.skip("resourcePackManager");
            return;
        }
        auto& tables = level->getLootTables();

        report.isTrue("vanillaFirst", tables.lookupByName(std::string{VanillaChest}, *resourcePacks) != nullptr);
        auto ours = registry.registerLootTableFromMemoryJson(std::string{VanillaChest}, std::string{DiamondTable});
        report.isTrue("registered", ours.has_value());
        report.isTrue(
            "served",
            ours.has_value() && tables.lookupByName(std::string{VanillaChest}, *resourcePacks) == ours.as_ptr()
        );
        report.isTrue("unregistered", registry.unregisterLootTable(std::string{VanillaChest}));
        auto* after = tables.lookupByName(std::string{VanillaChest}, *resourcePacks);
        report.isTrue("fallsBackToVanilla", after != nullptr && after != ours.as_ptr());
    });

    report.section("generate", [&] {
        // Actually roll a table: `LootTableUtils::fillContainer` resolves the name (through the hook, so
        // it gets the table this registry built), builds the loot context from the level and adds the
        // result to the container.
        auto level = ll::service::getLevel();
        if (!level) {
            report.skip("level");
            return;
        }

        auto table = registry.registerLootTableFromMemoryJson(std::string{TestGenerateDir}, std::string{DiamondTable});
        report.isTrue("registered", table.has_value());

        // A virtual player needs no client and is a real `Actor`, which is what the engine's loot context
        // needs. Creation happens at the shared spawn point, whose chunk is loaded.
        auto const spawn  = level->getSharedSpawnPos();
        auto       player = ::SimulatedPlayer::create(
            "ModApiTestLoot",
            ::Vec3{static_cast<float>(spawn.x) + 0.5f, static_cast<float>(spawn.y) + 2.0f, static_cast<float>(spawn.z) + 0.5f},
            ::DimensionType{0},
            ::Vec2{0.0f, 0.0f}
        );
        if (!player) {
            report.skip("simulatedPlayer");
            return;
        }

        // The engine's own runner builds the context and rolls the table.
        auto items = ::Util::LootTableUtils::generateRandomDeathLoot(*table, *player, nullptr, nullptr, nullptr, 0.0f);
        report.isTrue("producedSomething", !items.empty());
        report.isTrue("producedDiamond", !items.empty() && items.front().getTypeName() == "minecraft:diamond");

        player->remove();
    });
    report.section("unregister", [&] {
        report.isTrue("removed", registry.unregisterLootTable(std::string{TestJsonDir}));
        report.isTrue("gone", registry._findTable(std::string{TestJsonDir}) == nullptr);
        report.isTrue("unknown", !registry.unregisterLootTable(std::string{UnknownDir}));
    });

    report.section("reject", [&] {
        report.isTrue("emptyDir", !registry.registerLootTableFromMemoryJson("", std::string{DiamondTable}).has_value());
        report.isTrue(
            "malformed",
            !registry.registerLootTableFromMemoryJson(std::string{UnknownDir}, "{ not json").has_value()
        );
        report.isTrue(
            "missingFile",
            !registry.registerLootTableFromJsonFile(std::filesystem::path{"plugins/test/definitely-missing.json"})
                 .has_value()
        );
    });

    report.finish();
    return report.failed();
}

namespace {
void registerLootTableTestCommand() { registerTestFunction("loottable", [] { return runLootTableTests() == 0; }); }
} // namespace

REGISTER_ON_LOAD_TEST(LootTableFromLoad, registerAtLoad)
REGISTER_ON_LOAD_TEST(LootTableCommand, registerLootTableTestCommand)
REGISTER_TEST(LootTable, runLootTableTests)
