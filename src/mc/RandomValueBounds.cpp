#include <mc/world/level/storage/loot/RandomValueBounds.h>

// Definitions for symbols not exported in LeviLamina 26.51.x server (see src/mc/ItemInstance.cpp).
//
// `LootPool` stores its `rolls` and `bonus_rolls` as a `RandomValueBounds`, and the generated header
// only declares its constructors and copy assignment - filling a loot pool in code therefore needs
// them here. The layout is two floats (`min`, `max`), which LeviLamina emits as unnamed storage.
//
// The value is zeroed rather than `= default`, so a pool that does not set its bounds itself does not
// roll a garbage number of times; the loot table builder sets both bounds explicitly.

RandomValueBounds::RandomValueBounds() {
    mUnk98064d.as<float>() = 0.0f;
    mUnk79891e.as<float>() = 0.0f;
}

RandomValueBounds::RandomValueBounds(RandomValueBounds const& other) = default;

RandomValueBounds& RandomValueBounds::operator=(RandomValueBounds const& other) = default;
