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
