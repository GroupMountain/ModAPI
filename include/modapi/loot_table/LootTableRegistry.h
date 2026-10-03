#pragma once
#include "modapi/Macros.h"
#include "modapi/Registry.h"
#include "modapi/loot_table/base/ICustomLootTable.h"
#include <concepts>
#include <filesystem>
#include <functional>
#include <ll/api/event/Event.h>
#include <mc/deps/core/utility/optional_ref.h>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

class LootPoolEntry;
class LootTable;
class LootTables;
class ResourcePackManager;

namespace Json {
class Value;
}

namespace modapi::inline loot_table {

class LootTableRegistry;
class LootTableBuilder;

// Published the first time vanilla asks a `LootTables` instance for a table by name, which is the
// moment registered tables become reachable. Loot tables are not added to a container while a pass
// runs - `LootTables` resolves them on demand, keyed by the directory vanilla looks them up by
// ("loot_tables/chests/simple_dungeon.json") - so the event hands out the registry itself.
class LootTableReadyEvent final : public ll::event::Event {
    LootTableRegistry& mRegistry;

public:
    constexpr explicit LootTableReadyEvent(LootTableRegistry& registry) : mRegistry(registry) {}

    MOD_NDAPI LootTableRegistry& registry() const;
};

// Loot table support.
//
// Vanilla keeps loot tables in `LootTables::mLootTables`, filled from resource packs and resolved on
// demand by directory. This registry installs a hook on that single lookup point, so a table a mod
// registered is served instead of (or rather: before) the resource pack one, and every other engine
// path - `LootTable::fill`, `LootResolver`, `LootTableReference` entries - sees it too.
//
// Three ways to hand a table over, in decreasing order of convenience for the mod:
//
//   * `registerLootTableFromMemoryJson` / `registerLootTableFromJsonFile` - JSON text, parsed by the
//     engine's own `LootTable::deserialize`, so every entry, condition and function works;
//   * `registerLootTableFromJsonValue` - the same, with the document assembled as an engine DOM;
//   * `LootTableBuilder` / `registerLootTable` - built in code out of the engine's own loot types
//     (what the server build exposes; conditions and functions stay JSON-only for the engine).
//
// A registration only works once the engine has looked a table up (`isReady()`): the engine's
// `LootTable::deserialize` resolves items as it parses, so it cannot run while the server is still
// starting - and unlike the other registries there is no vanilla pass that is guaranteed to run before
// then (the first lookup can be well after the server started). Register through
// `DeferredRegister<LootTableRegistry, ...>`, which waits for `LootTableReadyEvent`.
class LootTableRegistry {
public:
    struct Impl;
    std::unique_ptr<Impl> pImpl;

public:
    using EventType  = LootTableReadyEvent;
    using Product    = ::LootTable;
    using ProductRef = optional_ref<::LootTable>;

public:
    LootTableRegistry();
    LootTableRegistry& operator=(LootTableRegistry const&) = delete;
    LootTableRegistry(LootTableRegistry const&)            = delete;

public:
    MOD_NDAPI static LootTableRegistry& getInstance();

    // True once vanilla asked for a loot table, i.e. once the tables are reachable.
    MOD_NDAPI bool isReady() const noexcept;


    // Registers the ready event emitter (idempotent); called by `ModAPI::load()` and by every
    // `DeferredRegister<LootTableRegistry, ...>`.
    MOD_API static void ensureEventRegistered();

    // Entry types are ICustomLootTable implementations.
    template <class Entry>
    static constexpr bool isEntry = std::derived_from<Entry, ICustomLootTable>;

    // Builds `Entry` and registers the table it describes.
    template <std::derived_from<ICustomLootTable> Entry, class... Args>
    ProductRef registerEntry(Args&&... args) {
        return _registerTable([... args = std::forward<Args>(args)]() -> std::unique_ptr<ICustomLootTable> {
            return std::make_unique<Entry>(args...);
        });
    }

    // `tableDir` is the name vanilla uses, e.g. "loot_tables/chests/simple_dungeon.json";
    // `formatVersion` is the JSON format the document is written against.
    MOD_NDAPI ProductRef registerLootTableFromMemoryJson(std::string const& tableDir, std::string const& rawJson);

    // Takes the directory from the path: everything from its "loot_tables" segment on, or the file
    // name below "loot_tables/" when the path has no such segment.
    MOD_NDAPI ProductRef registerLootTableFromJsonFile(std::filesystem::path const& jsonPath);

    // Builds a table with the engine's own `LootTable::deserialize` and stamps `tableDir` into it, so
    // `getDir` reports the directory vanilla looks the table up by. This is what an `ICustomLootTable`'s
    // `_init()` uses; it needs the engine to be up (see the class comment).
    MOD_NDAPI static std::unique_ptr<::LootTable> buildTable(std::string const& tableDir, std::string const& rawJson);

    // The engine's own DOM, for callers that assemble the document in code instead of parsing text.
    MOD_NDAPI ProductRef registerLootTableFromJsonValue(std::string const& tableDir, ::Json::Value const& table);

    // A table the caller built itself out of the engine's own types (`LootTableBuilder` below does
    // exactly that, and is the easier way in).
    MOD_NDAPI ProductRef registerLootTable(std::string const& tableDir, std::unique_ptr<::LootTable> table);

    // A table assembled by the builder, registered in one step (which consumes the builder).
    MOD_NDAPI ProductRef registerLootTable(std::string const& tableDir, LootTableBuilder& builder);

    // Builds the entry and registers the table it describes.
    MOD_NDAPI ProductRef _registerTable(std::function<std::unique_ptr<ICustomLootTable>()>&& factory);

    // Drops a table this registry owns, and the copy the engine cached for it.
    MOD_API bool unregisterLootTable(std::string const& tableDir);

    // The table for a directory: one a mod registered, otherwise vanilla's (the latter needs the
    // engine, so it only works once a lookup happened).
    MOD_NDAPI ProductRef getLootTable(std::string const& tableDir);

    // The table this registry owns for `tableDir`, or null.
    MOD_NDAPI Product* _findTable(std::string const& tableDir);

    // Remembers the instance vanilla is using and publishes `LootTableReadyEvent` once.
    MOD_API void _bindRegistry(::LootTables& tables, ::ResourcePackManager& resourcePackManager);
};

// Programmatic loot tables, so a mod does not have to ship JSON.
//
// The server build exposes the entries (`LootItem`, `LootTableReference`) with their weight, quality
// and count, plus `LootItemFunctions::deserialize` for the item functions, and ModAPI defines the few
// constructors the mod prelink does not provide (see `src/mc/RandomValueBounds.cpp`). Conditions stay
// JSON-only for the engine - `LootItemCondition::deserialize` is client-only/`MCNAPI` - so a table
// that needs them is better registered through the JSON APIs above.
class LootTableBuilder {
public:
    MOD_API LootTableBuilder();
    MOD_API ~LootTableBuilder();
    MOD_API                   LootTableBuilder(LootTableBuilder&&) noexcept;
    MOD_API LootTableBuilder& operator=(LootTableBuilder&&) noexcept;
    LootTableBuilder(LootTableBuilder const&)            = delete;
    LootTableBuilder& operator=(LootTableBuilder const&) = delete;

    // A pool holding one item entry; `minCount`/`maxCount` are inclusive.
    MOD_API LootTableBuilder&
    addItemPool(std::string itemName, int weight = 1, int quality = 0, int minCount = 1, int maxCount = 1);

    // A pool holding one entry that resolves another table by directory (through this registry).
    MOD_API LootTableBuilder& addReferencePool(std::string tableDir, int weight = 1, int quality = 0);

    // A pool holding an entry of your own - a `LootPoolEntry` subclass, e.g. one with its own
    // `_createItem`. The engine's JSON parser only knows its built-in entry, function and condition
    // types (their dispatchers are internal to the server build), so behaviour the built-in types do not
    // have is added here, in code. Custom functions and conditions ride along on the entry: build it,
    // fill its `mFunctions`/`mConditions` (or the pool's `mConditions`) and hand it over.
    MOD_API LootTableBuilder&
    addEntry(std::unique_ptr<::LootPoolEntry> entry, int weight = 1, int quality = 0, float rolls = 1.0f);

    [[nodiscard]] MOD_NDAPI bool   empty() const;
    [[nodiscard]] MOD_NDAPI size_t poolCount() const;

private:
    friend class LootTableRegistry;
    // The registry turns the builder into the engine table, so the engine types stay out of this
    // header. Handing the pools over means the builder can only be registered once.
    MOD_NDAPI std::unique_ptr<::LootTable> _build();

    struct Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace modapi::inline loot_table

static_assert(modapi::Registry<modapi::LootTableRegistry>);
