#include "modapi/item/base/ICustomItem.h"
#include "modapi/item/shared_types/ItemInitializer.h"
#include "modapi/item/shared_types/NetworkTagBuilder.h"
#include <ll/api/memory/Memory.h>
#include <mc/deps/puv/puv_load_data/LoadResultWithTiming.h>
#include <mc/world/item/ItemStackBase.h>
#include <mc/world/item/enchanting/EnchantUtils.h>
#include <mc/world/level/block/Block.h>

namespace modapi::inline item {

ICustomItem::ICustomItem(std::string const& identifier) : Item(identifier, 0) {
    mItemParseVersion = ItemVersion::DataDriven;
}

ICustomItem::~ICustomItem() = default;

uint8_t ICustomItem::getItemMaxStackSize() const { return 64; }

std::vector<std::string> ICustomItem::getItemTags() const { return {}; }

bool ICustomItem::allowOffhand() const { return false; }

std::string ICustomItem::getHoverTextColorFormat() const { return {}; }

bool ICustomItem::shouldDespawn() const { return true; }

::Enchant::Slot ICustomItem::getEnchantmentSlot() const { return ::Enchant::Slot::None; }

bool ICustomItem::isFoil() const { return false; }

DamageChance ICustomItem::getItemDamageChance() const { return DamageChance(100, 100); }

short ICustomItem::getItemDurability() const { return 0; }

std::vector<RepairItems> ICustomItem::getRepairItems() const { return {}; }

::SharedTypes::Legacy::UseAnimation ICustomItem::getUseAnimation() const {
    return ::SharedTypes::Legacy::UseAnimation::None;
}

bool ICustomItem::requiresWorldBuilder() const { return false; }

bool ICustomItem::isExplodable() const { return true; }

bool ICustomItem::isFireResistant() const { return false; }

bool ICustomItem::shouldIgnoresPermissions() const { return false; }

bool ICustomItem::shouldAnimatesInToolbar() const { return false; }

std::string ICustomItem::getDisplayName() const { return {}; }

int ICustomItem::getUseDuration() const { return 0; }

::Interactions::Mining::MineBlockItemEffectType ICustomItem::getMineBlockItemEffectType() const {
    return ::Interactions::Mining::MineBlockItemEffectType::Default;
}

::CreativeItemCategory ICustomItem::getCreativeCategory() const { return ::CreativeItemCategory::Items; }

::std::string ICustomItem::getCreativeGroup() const { return {}; }

float ICustomItem::getMiningSpeed() const { return 1.0f; }

bool ICustomItem::isFuel() const { return false; }

uint8_t ICustomItem::getCompostChance() const { return 0; }

float ICustomItem::getFurnaceBurnInterval() const { return 0; }

float ICustomItem::getFurnaceXPmultiplier() const { return 0; }

::ItemCommandVisibility ICustomItem::shouldHiddenInCommands() const { return ::ItemCommandVisibility::Visible; }

bool ICustomItem::isSmithingTemplate() const { return false; }

bool ICustomItem::isSmithingTransformable() const { return false; }

bool ICustomItem::isSmithingTransformMaterial() const { return false; }

bool ICustomItem::canDestroyInCreative() const { return true; }

bool ICustomItem::isDiggerItem() const { return false; }

float ICustomItem::getDestroySpeed(::ItemStackBase const& item, ::Block const& block) const {
    if (isDiggerItem() && !item.isNull() && canDestroySpecial(block)) {
        auto level = EnchantUtils::getEnchantLevel(::Enchant::Type::Efficiency, item);
        return getMiningSpeed() + (float)(level * level) + 1.0f;
    }
    return 1.0f;
}

int ICustomItem::getFrameCount() const { return 1; }

std::unique_ptr<::CompoundTag> ICustomItem::buildNetworkTag() const { return buildClientComponents(*this); }

int ICustomItem::getEnchantSlot() const { return (int)getEnchantmentSlot(); }

bool ICustomItem::isComponentBased() const { return false; }

int ICustomItem::getDamageChance(int unbreaking) const {
    auto origin = Item::getDamageChance(unbreaking);
    return origin * getItemDamageChance().random() / 100;
}

short ICustomItem::getMaxDamage() const { return getItemDurability(); }

void ICustomItem::_init() { return initCustomItem(*this); }

bool ICustomItem::isValidRepairItem(::ItemStackBase const&, ::ItemStackBase const& repairItem, ::BaseGameVersion const&)
    const {
    for (auto& items : getRepairItems()) {
        for (auto& item : items.mItems) {
            if (item == repairItem.getTypeName()) {
                return true;
            }
        }
    }
    return false;
}

::HashedString const& ICustomItem::getCooldownCategory() const {
    static HashedString empty;
    return empty;
}

std::string ICustomItem::getInteractButtonText() const { return "action.interact.use"; }

std::string ICustomItem::getInteractText(::Player const&) const { return getInteractButtonText(); }

PuvLoadData::LoadResultWithTiming ICustomItem::initServer(
    Json::Value const&  json,
    SemVersion const&   version,
    IPackLoadContext&   context,
    JsonBetaState const state
) {
    return Item::initServer(json, version, context, state);
}

PuvLoadData::LoadResultWithTiming ICustomItem::initClient(
    Json::Value const&  json,
    SemVersion const&   version,
    JsonBetaState const state,
    IPackLoadContext&   context
) {
    return Item::initClient(json, version, state, context);
}

} // namespace modapi::inline item

// 临时修复链接问题

PuvLoadData::LoadResultWithTiming Item::initServer(
    Json::Value const&  json,
    SemVersion const&   version,
    IPackLoadContext&   context,
    JsonBetaState const state
) {
    // clang-format off
    using namespace ll::memory_literals;
    using FuncType = PuvLoadData::LoadResultWithTiming (*)(
        Item*,
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

PuvLoadData::LoadResultWithTiming Item::initClient(
    Json::Value const&  json,
    SemVersion const&   version,
    JsonBetaState const state,
    IPackLoadContext&   context
) {
    // clang-format off
    using namespace ll::memory_literals;
    using FuncType = PuvLoadData::LoadResultWithTiming (*)(
        Item*,
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