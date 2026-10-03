#pragma once
#include "modapi/Macros.h"
#include "modapi/item/types/DamageChance.h"
#include "modapi/item/types/ItemIcon.h"
#include "modapi/item/types/RepairItems.h"
#include <mc/deps/nbt/CompoundTag.h>
#include <mc/deps/shared_types/legacy/item/UseAnimation.h>
#include <mc/deps/shared_types/v1_26_30/item/ItemDocument.h>
#include <mc/world/actor/DefinitionEvent.h>
#include <mc/world/item/Item.h>
#include <mc/world/item/ItemComprehensiveLoadResult.h>
#include <mc/world/item/ItemIconInfo.h>
#include <mc/world/item/ItemStackBase.h>
#include <mc/world/item/Rarity.h>
#include <mc/world/item/components/LegacyEventItemComponentData.h>
#include <mc/world/item/enchanting/Enchant.h>
#include <mc/world/item/enchanting/EnchantUtils.h>
#include <mc/world/level/block/Block.h>


namespace modapi::inline item {

// The answers a mod gives an item, with no engine base of its own: every hook below is the default a mod gets when it
// overrides nothing, and each one is defined here, in the class, rather than in a `.cpp` - a mod compiles its own
// definitions, so nothing has to be exported for a mod's subclass to link.
//
// This is deliberately *not* the engine class. `ICustomItem<T>` is what brings in an engine base, and it can only be
// written on top of this one: a template member's definition has to be visible in every translation unit that
// instantiates it, and the mangled name of `ICustomItem<T>::someHook` changes with `T`, so nothing here can live in a
// `.cpp` either way. Keeping the hooks in a non-template base means the two helpers that do the heavy work
// (`initCustomItem`, `buildClientComponents`) are ordinary exported functions rather than one copy per `T`.
// The two helpers a template member below calls. They live in `modapi/item/shared_types/...` (which include this
// header, so this is a forward declaration on purpose), and their definitions must be visible in every translation
// unit that instantiates `ICustomItem<T>` - that is what makes them reusable for any `T` at all.
template <class ItemT>
void initCustomItem(ItemT& item);

template <class ItemT>
std::unique_ptr<CompoundTag> buildClientComponents(ItemT const& item);

class ICustomItemBase {
public:
    virtual ~ICustomItemBase() = default;

    // What a client draws in the inventory. A mod has to answer this one: an item without an icon, and without a
    // block to draw instead, shows up blank.
    virtual ItemIcon getIcon() const = 0;

    virtual uint8_t getItemMaxStackSize() const { return 64; }

    virtual std::vector<std::string> getItemTags() const { return {}; }

    virtual bool allowOffhand() const { return false; }

    virtual std::string getHoverTextColorFormat() const { return {}; }

    virtual bool shouldDespawn() const { return true; }

    virtual ::Enchant::Slot getEnchantmentSlot() const { return ::Enchant::Slot::None; }

    virtual bool isFoil() const { return false; }

    virtual uint8_t getCompostChance() const { return 0; }

    virtual DamageChance getItemDamageChance() const { return DamageChance(100, 100); }

    virtual short getItemDurability() const { return 0; }

    virtual std::vector<RepairItems> getRepairItems() const { return {}; }

    virtual ::SharedTypes::Legacy::UseAnimation getUseAnimation() const {
        return ::SharedTypes::Legacy::UseAnimation::None;
    }

    virtual bool requiresWorldBuilder() const { return false; }

    virtual bool isExplodable() const { return true; }

    virtual bool isFireResistant() const { return false; }

    virtual bool shouldIgnoresPermissions() const { return false; }

    virtual bool shouldAnimatesInToolbar() const { return false; }

    virtual std::string getDisplayName() const { return {}; }

    virtual int getUseDuration() const { return 0; }

    virtual ::Interactions::Mining::MineBlockItemEffectType getMineBlockItemEffectType() const {
        return ::Interactions::Mining::MineBlockItemEffectType::Default;
    }

    virtual ::SharedTypes::CreativeItemCategory getCreativeCategory() const {
        return ::SharedTypes::CreativeItemCategory::Items;
    }

    virtual ::std::string getCreativeGroup() const { return {}; }

    virtual bool isDiggerItem() const { return false; }

    virtual float getMiningSpeed() const { return 1.0f; }

    virtual bool isFuel() const { return false; }

    virtual float getFurnaceBurnInterval() const { return 0; }

    virtual ::ItemCommandVisibility shouldHiddenInCommands() const { return ::ItemCommandVisibility::Visible; }

    virtual bool isSmithingTransformable() const { return false; }

    virtual bool isSmithingTransformMaterial() const { return false; }

    virtual bool isSmithingTemplate() const { return false; }

    virtual int getFrameCount() const { return 1; }

    virtual std::string getInteractButtonText() const { return "action.interact.use"; }
};

// A mod's item: the engine class it stands for, plus the hooks above. `class MyItem : public ICustomItem<::Item>` is
// the ordinary case; an engine subclass (`::TridentItem`, `::BlockItem`, ...) is just a different `T`.
//
// Only the members that override an engine virtual are here, because only they have to be written against `T` - and
// every one of them is a one line call into a helper that lives in a `.cpp` (or into `T`'s own implementation), so
// this class stays readable while the work stays in one place.
template <typename T>
class ICustomItem : public T, public ICustomItemBase {
public:
    explicit ICustomItem(std::string const& identifier) : T(identifier, 0) {
        // Required, not decoration: it makes the engine treat the item as a data driven one, which is what gives it
        // the definition machinery a client needs. It was commented out during an experiment and left that way, and
        // custom items came up blank on a client from then on.
        mItemParseVersion = ItemVersion::DataDriven;
    }

    // For an engine base whose constructor takes more than the identifier (a block item needs its block, for
    // instance). The arguments are whatever `T` asks for, the identifier included.
    template <class... Args>
    explicit ICustomItem(Args&&... args) : T(std::forward<Args>(args)...) {
        mItemParseVersion = ItemVersion::DataDriven;
    }

    ~ICustomItem() override = default;

    // Not a virtual: the registry calls it on the type it just built, so a mod's own `_init` is reached by name
    // rather than through a vtable slot - which is what lets it stay out of the engine-facing signature set.
    void _init() { initCustomItem(*this); }

    std::unique_ptr<CompoundTag> buildNetworkTag() const override { return buildClientComponents(*this); }

    float getDestroySpeed(::ItemStackBase const& item, ::Block const& block) const override {
        if (isDiggerItem() && !item.isNull() && canDestroySpecial(block)) {
            auto level = ::EnchantUtils::getEnchantLevel(::Enchant::Type::Efficiency, item);
            return getMiningSpeed() + (float)(level * level) + 1.0f;
        }
        return 1.0f;
    }

    bool isValidRepairItem(
        ::ItemStackBase const&,
        ::ItemStackBase const& repairItem,
        ::BaseGameVersion const&
    ) const override {
        for (auto& items : getRepairItems()) {
            for (auto& item : items.mItems) {
                if (item == repairItem.getTypeName()) {
                    return true;
                }
            }
        }
        return false;
    }

    int getDamageChance(int unbreaking) const override {
        // `T::` on purpose: an unqualified call would come back here.
        return T::getDamageChance(unbreaking) * getItemDamageChance().random() / 100;
    }

    void initServer(
        ::SharedTypes::v1_26_30::ItemDocument&&         data,
        ::SemVersion const&                             documentVersion,
        ::Experiments const&                            experiments,
        ::std::optional<::LegacyEventItemComponentData> legacyEventData
    ) override {
        T::initServer(std::move(data), documentVersion, experiments, legacyEventData);
    }

    void initClient(
        ::SharedTypes::v1_26_30::ItemDocument&& data,
        ::SemVersion const&                     documentVersion,
        ::Experiments const&                    experiments,
        ::std::optional<::ItemIconInfo> (*iconFactory)(::std::string const&, int)
    ) override {
        T::initClient(std::move(data), documentVersion, experiments, iconFactory);
    }

    ::HashedString const& getCooldownCategory() const override {
        static ::HashedString empty;
        return empty;
    }

    float getFurnaceXPmultiplier(::ItemStackBase const&) const override { return 0; }

    bool canDestroyInCreative() const override { return true; }

    int getEnchantSlot() const override { return (int)getEnchantmentSlot(); }

    short getMaxDamage() const override { return getItemDurability(); }

    bool isComponentBased() const override { return false; }

    std::string getInteractText(::Player const&) const override { return getInteractButtonText(); }
};

} // namespace modapi::inline item
