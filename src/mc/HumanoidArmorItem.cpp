#include "modapi/item/types/mc/HumanoidArmorItem.h"
#include <ll/api/memory/Memory.h>
#include <magic_enum.hpp>
#include <mc/deps/puv/puv_load_data/LoadResultWithTiming.h>

LL_FORCEINLINE HumanoidArmorItem::ArmorMaterial const& getArmorMaterial(HumanoidArmorItem::Tier armorTier) {
    switch (armorTier) {
    case ::HumanoidArmorItem::Tier::Chain:
        return HumanoidArmorItem::CHAIN();
    case ::HumanoidArmorItem::Tier::Copper:
        return HumanoidArmorItem::COPPER();
    case ::HumanoidArmorItem::Tier::Iron:
        return HumanoidArmorItem::IRON();
    case ::HumanoidArmorItem::Tier::Diamond:
        return HumanoidArmorItem::DIAMOND();
    case ::HumanoidArmorItem::Tier::Gold:
        return HumanoidArmorItem::GOLD();
    case ::HumanoidArmorItem::Tier::Elytra:
        return HumanoidArmorItem::ELYTRA();
    case ::HumanoidArmorItem::Tier::Turtle:
        return HumanoidArmorItem::TURTLE();
    case ::HumanoidArmorItem::Tier::Netherite:
        return HumanoidArmorItem::NETHERITE();
    default:
        return HumanoidArmorItem::LEATHER();
    }
    static_assert(magic_enum::enum_count<HumanoidArmorItem::Tier>() == 9, "Unhandled HumanoidArmorItem::Tier value");
}

HumanoidArmorItem::HumanoidArmorItem(std::string const& name, HumanoidArmorItem::Tier armorTier)
: Item(name, 0),
  mArmorType(getArmorMaterial(armorTier)),
  mCurrentVersionAllowsTrim(true) {
    if (armorTier == HumanoidArmorItem::Tier::Netherite) {
        mFireResistant = true;
    }
}

// 临时修复链接问题

PuvLoadData::LoadResultWithTiming HumanoidArmorItem::initServer(
    Json::Value const&  json,
    SemVersion const&   version,
    IPackLoadContext&   context,
    JsonBetaState const state
) {
    // clang-format off
    using namespace ll::memory_literals;
    using FuncType = PuvLoadData::LoadResultWithTiming (*)(
        HumanoidArmorItem*,
        Json::Value const&,
        SemVersion const&,
        IPackLoadContext&,
        JsonBetaState
    );
    static auto initFunc = reinterpret_cast<FuncType>(
        "48 89 5C 24 ?? 55 56 57 41 54 "
        "41 55 41 56 41 57 48 8D 6C 24 "
        "?? 48 81 EC ?? ?? ?? ?? 0F 29 "
        "B4 24 ?? ?? ?? ?? 48 8B 05 ?? "
        "?? ?? ?? 48 33 C4 48 89 45 ?? "
        "4D 8B F8 48 8B FA 48 8B F1"_sig.resolve()
    );
    return initFunc(this, json, version, context, state);
    // clang-format on
}

PuvLoadData::LoadResultWithTiming HumanoidArmorItem::initClient(
    Json::Value const&  json,
    SemVersion const&   version,
    JsonBetaState const state,
    IPackLoadContext&   context
) {
    // clang-format off
    using namespace ll::memory_literals;
    using FuncType = PuvLoadData::LoadResultWithTiming (*)(
        HumanoidArmorItem*,
        Json::Value const&,
        SemVersion const&,
        JsonBetaState const,
        IPackLoadContext&
    );
    static auto initFunc = reinterpret_cast<FuncType>(
        "48 89 5C 24 ?? 55 56 57 41 54 "
        "41 55 41 56 41 57 48 8D 6C 24 "
        "?? 48 81 EC ?? ?? ?? ?? 0F 29 "
        "B4 24 ?? ?? ?? ?? 48 8B 05 ?? "
        "?? ?? ?? 48 33 C4 48 89 45 ?? "
        "4D 8B F8 48 8B F2"_sig.resolve()
    );
    return initFunc(this, json, version, state, context);
    // clang-format on
}