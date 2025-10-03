#include "modapi/item/base/ICustomArmorItem.h"
#include "modapi/item/shared_types/ItemInitializer.h"
#include "modapi/item/shared_types/NetworkTagBuilder.h"
#include <mc/world/actor/player/Player.h>
#include <mc/world/item/ItemStack.h>
#include <mc/world/item/ItemStackBase.h>

HumanoidArmorItem::ArmorMaterial const& getArmorMaterial(HumanoidArmorItem::Tier armorTier) {
    switch (armorTier) {
    case ::HumanoidArmorItem::Tier::Chain:
        return HumanoidArmorItem::CHAIN();
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
    }
    return HumanoidArmorItem::LEATHER();
}

HumanoidArmorItem::HumanoidArmorItem(std::string const& name, HumanoidArmorItem::Tier armorTier)
: Item(name, 0),
  mArmorType(getArmorMaterial(armorTier)),
  mCurrentVersionAllowsTrim(true) {
    if (armorTier == HumanoidArmorItem::Tier::Netherite) {
        mFireResistant = true;
    }
}

namespace modapi::inline item {

ICustomArmorItem::ICustomArmorItem(std::string const& identifier, HumanoidArmorItem::Tier armorTier)
: HumanoidArmorItem(identifier, armorTier),
  pImpl(std::make_unique<Impl>()) {
    mItemParseVersion = ItemVersion::DataDriven;
}

ICustomArmorItem::~ICustomArmorItem() = default;

int ICustomArmorItem::getModelIndex() const { return 0; }

uint8_t ICustomArmorItem::getItemMaxStackSize() const { return 1; }

std::vector<std::string> ICustomArmorItem::getItemTags() const { return {}; }

bool ICustomArmorItem::allowOffhand() const { return false; }

std::string ICustomArmorItem::getHoverTextColorFormat() const { return {}; }

bool ICustomArmorItem::shouldDespawn() const { return true; }

::Enchant::Slot ICustomArmorItem::getEnchantmentSlot() const { return ::Enchant::Slot::None; }

bool ICustomArmorItem::isFoil() const { return false; }

uint8_t ICustomArmorItem::getCompostChance() const { return 0; }

DamageChance ICustomArmorItem::getItemDamageChance() const { return DamageChance(100, 100); }

short ICustomArmorItem::getItemDurability() const { return 0; }

std::vector<RepairItems> ICustomArmorItem::getRepairItems() const { return {}; }

::SharedTypes::Legacy::UseAnimation ICustomArmorItem::getUseAnimation() const {
    return ::SharedTypes::Legacy::UseAnimation::None;
}

bool ICustomArmorItem::requiresWorldBuilder() const { return false; }

bool ICustomArmorItem::isExplodable() const { return true; }

bool ICustomArmorItem::isFireResistant() const { return false; }

bool ICustomArmorItem::shouldIgnoresPermissions() const { return false; }

bool ICustomArmorItem::shouldAnimatesInToolbar() const { return false; }

std::string ICustomArmorItem::getDisplayName() const { return {}; }

int ICustomArmorItem::getUseDuration() const { return 0; }

::Interactions::Mining::MineBlockItemEffectType ICustomArmorItem::getMineBlockItemEffectType() const {
    return ::Interactions::Mining::MineBlockItemEffectType::Default;
}

::CreativeItemCategory ICustomArmorItem::getCreativeCategory() const { return ::CreativeItemCategory::Items; }

::std::string ICustomArmorItem::getCreativeGroup() const { return {}; }

bool ICustomArmorItem::isFuel() const { return false; }

float ICustomArmorItem::getFurnaceBurnInterval() const { return 0; }

float ICustomArmorItem::getFurnaceXPmultiplier() const { return 0; }

::ItemCommandVisibility ICustomArmorItem::shouldHiddenInCommands() const { return ::ItemCommandVisibility::Visible; }

bool ICustomArmorItem::isSmithingTemplate() const { return false; }

bool ICustomArmorItem::isSmithingTransformable() const { return false; }

bool ICustomArmorItem::isSmithingTransformMaterial() const { return false; }

bool ICustomArmorItem::canDestroyInCreative() const { return true; }

float ICustomArmorItem::getMiningSpeed() const { return 1.0f; }

int ICustomArmorItem::getFrameCount() const { return 1; }

std::unique_ptr<::CompoundTag> ICustomArmorItem::buildNetworkTag() const { return buildClientComponents(*this); }

int ICustomArmorItem::getArmorValue() const { return 0; }

int ICustomArmorItem::getToughnessValue() const { return 0; }

float ICustomArmorItem::getArmorKnockbackResistance() const { return 0; }

bool ICustomArmorItem::isDyeable() const { return false; }

bool ICustomArmorItem::isTrimAllowed() const { return false; }

int ICustomArmorItem::getEnchantSlot() const { return (int)getEnchantmentSlot(); }

bool ICustomArmorItem::isComponentBased() const { return false; }

int ICustomArmorItem::getDamageChance(int unbreaking) const {
    auto origin = Item::getDamageChance(unbreaking);
    return origin * getItemDamageChance().random() / 100;
}

short ICustomArmorItem::getMaxDamage() const { return getItemDurability(); }

void ICustomArmorItem::_init() { return initCustomItem(*this); }

bool ICustomArmorItem::
    isValidRepairItem(::ItemStackBase const&, ::ItemStackBase const& repairItem, ::BaseGameVersion const&) const {
    for (auto& items : getRepairItems()) {
        for (auto& item : items.mItems) {
            if (item == repairItem.getTypeName()) {
                return true;
            }
        }
    }
    return false;
}

::std::string ICustomArmorItem::getCooldownCategory() const { return {}; }

const HashedString& ICustomArmorItem::getCooldownType() const {
    if (!getCooldownCategory().empty()) {
        return pImpl->mCooldownCategory;
    }
    return Item::getCooldownType();
}

std::string ICustomArmorItem::getInteractButtonText() const { return "action.interact.use"; }

std::string ICustomArmorItem::getInteractText(::Player const&) const { return getInteractButtonText(); }

::ItemStack& ICustomArmorItem::use(::ItemStack& item, ::Player& player) const {
    auto  oldItem = item;
    auto& result  = HumanoidArmorItem::use(item, player);
    if (player.isCreative() && result.isNull()) {
        result = oldItem;
    }
    return result;
}

} // namespace modapi::inline item