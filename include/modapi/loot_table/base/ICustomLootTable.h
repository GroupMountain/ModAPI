#pragma once
#include "modapi/Macros.h"
#include <memory>
#include <string>

class LootTable;

namespace modapi::inline loot_table {

class LootTableRegistry;

// A custom loot table, in the same shape as the other entry types (`ICustomItem`, `ICustomRecipe`,
// `ICustomGameRule`, ...): the mod describes what it wants and `_init()` produces the engine object,
// which the registry then registers.
//
//   class MyLootTable : public modapi::loot_table::ICustomLootTable {
//   public:
//       std::unique_ptr<::LootTable> _init() override {
//           return modapi::LootTableRegistry::buildTable(
//               "loot_tables/my_mod/my_table.json",
//               R"({"pools":[{"rolls":1,"entries":[{"type":"item","name":"minecraft:diamond"}]}]})"
//           );
//       }
//   };
//   static modapi::DeferredRegister<modapi::LootTableRegistry, MyLootTable> gMyLootTable{};
//
// `_init()` runs when the engine first looks a loot table up (`LootTableReadyEvent`), i.e. at the
// earliest moment the engine's deserializer may run at all. Tables can also be registered without an
// entry type - see `LootTableRegistry`'s JSON and `LootTableBuilder` interfaces.
class ICustomLootTable {
public:
    ICustomLootTable()                                   = default;
    ICustomLootTable& operator=(ICustomLootTable const&) = delete;
    ICustomLootTable(ICustomLootTable const&)            = delete;

public:
    MOD_API virtual ~ICustomLootTable();

    // Builds the table to register. `LootTableRegistry::buildTable` hands the document to the engine's
    // own `LootTable::deserialize` (so every entry, condition and function works) and stamps the
    // directory vanilla looks the table up by into it; `LootTableBuilder` is there for tables that are
    // assembled in code instead.
    virtual std::unique_ptr<::LootTable> _init() = 0;
};

} // namespace modapi::inline loot_table
