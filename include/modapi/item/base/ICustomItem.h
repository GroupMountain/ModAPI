#pragma once
#include "modapi/Macros.h"
#include <map>
#include <mc/deps/core/string/HashedString.h>
#include <mc/deps/nbt/CompoundTag.h>
#include <mc/deps/shared_types/beta/item/UseModifiersItemComponent.h>
#include <mc/deps/shared_types/v1_20_50/item/CooldownItemComponent.h>
#include <mc/deps/shared_types/v1_20_50/item/FoodItemComponent.h>
#include <mc/deps/shared_types/v1_21_110/item/ItemCategory.h>
#include <mc/deps/shared_types/v1_21_110/item/ItemDescription.h>
#include <mc/deps/shared_types/v1_21_110/item/ItemMenuCategory.h>
#include <mc/deps/shared_types/v1_21_80/item/IconItemComponent.h>
#include <mc/deps/shared_types/v1_21_90/item/WearableItemComponent.h>
#include <mc/deps/shared_types/v1_26_0/item/PlanterItemComponent.h>
#include <mc/deps/shared_types/v1_26_30/item/UseModifiersItemComponent.h>
#include <mc/world/item/Item.h>
#include <mc/world/item/components/AllowOffHandItemComponent.h>
#include <mc/world/item/components/CanDestroyInCreativeItemComponent.h>
#include <mc/world/item/components/ComponentItem.h>
#include <mc/world/item/components/CompostableItemComponent.h>
#include <mc/world/item/components/CooldownItemComponent.h>
#include <mc/world/item/components/DamageItemComponent.h>
#include <mc/world/item/components/DisplayNameItemComponent.h>
#include <mc/world/item/components/DurabilityItemComponent.h>
#include <mc/world/item/components/EnchantableItemComponent.h>
#include <mc/world/item/components/FoodItemComponent.h>
#include <mc/world/item/components/FuelItemComponent.h>
#include <mc/world/item/components/GlintItemComponent.h>
#include <mc/world/item/components/HandEquippedItemComponent.h>
#include <mc/world/item/components/IconItemComponent.h>
#include <mc/world/item/components/ItemComponent.h>
#include <mc/world/item/components/MaxStackSizeItemComponent.h>
#include <mc/world/item/components/PlanterItemComponent.h>
#include <mc/world/item/components/RarityItemComponent.h>
#include <mc/world/item/components/TagsItemComponent.h>
#include <mc/world/item/components/UseModifiersItemComponent.h>
#include <mc/world/item/components/WearableItemComponent.h>
#include <mc/world/level/Level.h>
#include <mc/world/level/block/registry/BlockTypeRegistry.h>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace modapi::inline item {

MOD_API ::SharedTypes::v1_21_110::ItemDescription* createItemDescription();
MOD_API                                            std::shared_ptr<::WearableItemComponent>
makeWearableItemComponent(::SharedTypes::Legacy::EquipmentSlot slot, int protection);
MOD_API std::shared_ptr<::PlanterItemComponent>
makePlanterItemComponent(std::string block, bool canUseAsIcon, bool replaceBlockItem);

template <class T>
class ICustomItem : public T {
public:
    using CustomItemBase = T;
    using ItemComponents = std::map<std::string, std::shared_ptr<::ItemComponent>>;
    using T::canDestroyInCreative;

private:
    std::unique_ptr<::ComponentItem>                           mComponentItem;
    ItemComponents                                             mComponents;
    std::unique_ptr<::SharedTypes::v1_21_110::ItemDescription> mDescription;

public:
    explicit ICustomItem(std::string const& identifier) : T(identifier, 0), mDescription(createItemDescription()) {
        this->mItemParseVersion = ::ItemVersion::DataDriven;
    }

    template <class... Args>
    explicit ICustomItem(Args&&... args) : T(std::forward<Args>(args)...),
                                           mDescription(createItemDescription()) {
        this->mItemParseVersion = ::ItemVersion::DataDriven;
    }

    ~ICustomItem() override = default;

    [[nodiscard]] ItemComponents const& components() const noexcept { return mComponents; }

    ICustomItem& components(ItemComponents value) {
        mComponents = std::move(value);
        return *this;
    }

    ICustomItem& addComponent(std::string name, std::shared_ptr<::ItemComponent> component) {
        mComponents[std::move(name)] = std::move(component);
        return *this;
    }

    [[nodiscard]] ::SharedTypes::v1_21_110::ItemDescription const& description() const noexcept {
        return *mDescription;
    }

    ICustomItem& description(::SharedTypes::v1_21_110::ItemDescription const& value) {
        *mDescription->mIdentifier = *value.mIdentifier;
        *mDescription->mCategory   = *value.mCategory;
        return *this;
    }

    [[nodiscard]] ::SharedTypes::v1_21_110::ItemMenuCategory const& menuCategory() const noexcept {
        return *mDescription->mCategory;
    }

    ICustomItem& menuCategory(::SharedTypes::v1_21_110::ItemMenuCategory value) {
        *mDescription->mCategory = std::move(value);
        return *this;
    }

    [[nodiscard]] ::SharedTypes::v1_21_110::ItemCategory::CreativeItemCategory creativeCategory() const noexcept {
        return (*mDescription->mCategory).mCreativeCategory;
    }

    ICustomItem& creativeCategory(::SharedTypes::v1_21_110::ItemCategory::CreativeItemCategory value) {
        (*mDescription->mCategory).mCreativeCategory = value;
        return *this;
    }

    [[nodiscard]] std::string creativeGroup() const {
        return (*mDescription->mCategory).mCreativeGroupName->value_or("");
    }

    ICustomItem& creativeGroup(std::string value) {
        (*mDescription->mCategory).mCreativeGroupName = std::move(value);
        return *this;
    }

    [[nodiscard]] bool hiddenInCommands() const noexcept {
        return (*mDescription->mCategory).mIsHiddenInCommands->value_or(false);
    }

    ICustomItem& hiddenInCommands(bool value) {
        (*mDescription->mCategory).mIsHiddenInCommands = value;
        return *this;
    }

    [[nodiscard]] std::vector<std::string> tags() const {
        auto it = mComponents.find("minecraft:tags");
        if (it == mComponents.end() || !it->second) return {};
        return *static_cast<::TagsItemComponent*>(it->second.get())->mTags;
    }

    ICustomItem& tags(std::vector<std::string> value) {
        auto component    = std::make_shared<::TagsItemComponent>();
        *component->mTags = std::move(value);
        return addComponent("minecraft:tags", std::move(component));
    }

    ICustomItem& icon(std::string texture) {
        ::SharedTypes::v1_21_80::IconItemComponent payload{};
        auto                                       component = std::make_shared<::IconItemComponent>(payload);
        component->setTexturePath(texture);
        return addComponent("minecraft:icon", std::move(component));
    }

    ICustomItem& maxStackSize(uint8_t value) {
        auto component           = std::make_shared<::MaxStackSizeItemComponent>();
        component->mMaxStackSize = value;
        return addComponent("minecraft:max_stack_size", std::move(component));
    }

    ICustomItem& durability(int maxDamage, int minChance = 100, int maxChance = 100) {
        auto component                 = std::make_shared<::DurabilityItemComponent>();
        component->mMaxDamage          = maxDamage;
        component->mDamageChance->mMin = minChance;
        component->mDamageChance->mMax = maxChance;
        return addComponent("minecraft:durability", std::move(component));
    }

    ICustomItem& damage(int value) {
        auto component     = std::make_shared<::DamageItemComponent>();
        component->mDamage = static_cast<short>(value);
        return addComponent("minecraft:damage", std::move(component));
    }

    ICustomItem& displayName(std::string descriptionId) {
        auto component            = std::make_shared<::DisplayNameItemComponent>();
        component->mDescriptionId = std::move(descriptionId);
        return addComponent("minecraft:display_name", std::move(component));
    }

    ICustomItem& rarity(std::string value) {
        auto component = std::make_shared<::RarityItemComponent>();
        if (value == "uncommon") {
            component->mRarity = ::Rarity::Uncommon;
        } else if (value == "rare") {
            component->mRarity = ::Rarity::Rare;
        } else if (value == "epic") {
            component->mRarity = ::Rarity::Epic;
        } else {
            component->mRarity = ::Rarity::Common;
        }
        return addComponent("minecraft:rarity", std::move(component));
    }

    ICustomItem& glint(bool value = true) {
        auto component      = std::make_shared<::GlintItemComponent>();
        component->mIsGlint = value;
        return addComponent("minecraft:glint", std::move(component));
    }

    ICustomItem& handEquipped(bool value = true) {
        auto component           = std::make_shared<::HandEquippedItemComponent>();
        component->mHandEquipped = value;
        return addComponent("minecraft:hand_equipped", std::move(component));
    }

    ICustomItem& offHandAllowed(bool value = true) {
        auto component           = std::make_shared<::AllowOffHandItemComponent>();
        component->mAllowOffHand = value;
        return addComponent("minecraft:allow_off_hand", std::move(component));
    }

    ICustomItem& canDestroyInCreative(bool value = true) {
        auto component                   = std::make_shared<::CanDestroyInCreativeItemComponent>();
        component->mCanDestroyInCreative = value;
        return addComponent("minecraft:can_destroy_in_creative", std::move(component));
    }

    ICustomItem& enchantable(std::string slot, int value) {
        auto component           = std::make_shared<::EnchantableItemComponent>();
        component->mEnchantSlot  = std::move(slot);
        component->mEnchantValue = static_cast<uchar>(value);
        return addComponent("minecraft:enchantable", std::move(component));
    }

    ICustomItem& fuel(float duration) {
        auto component           = std::make_shared<::FuelItemComponent>();
        component->mFuelDuration = duration;
        return addComponent("minecraft:fuel", std::move(component));
    }

    ICustomItem& compostable(uint8_t chance) {
        auto component               = std::make_shared<::CompostableItemComponent>();
        component->mCompostingChance = static_cast<schar>(chance);
        return addComponent("minecraft:compostable", std::move(component));
    }

    ICustomItem& food(int nutrition, float saturationModifier, bool canAlwaysEat = false) {
        ::SharedTypes::v1_20_50::FoodItemComponent payload{};
        payload.mNutrition          = nutrition;
        payload.mSaturationModifier = saturationModifier;
        payload.mCanAlwaysEat       = canAlwaysEat;
        return addComponent("minecraft:food", std::make_shared<::FoodItemComponent>(payload));
    }

    ICustomItem& wearable(::SharedTypes::Legacy::EquipmentSlot slot, int protection) {
        return addComponent("minecraft:wearable", makeWearableItemComponent(slot, protection));
    }

    ICustomItem& blockPlacer(std::string block, bool canUseAsIcon = true, bool replaceBlockItem = true) {
        if (auto const* type = ::BlockTypeRegistry::get().lookupByName(::HashedString{block}, false)) {
            this->mBlockType = type;
        }
        return addComponent(
            "minecraft:block_placer",
            makePlanterItemComponent(std::move(block), canUseAsIcon, replaceBlockItem)
        );
    }

    ICustomItem& cooldown(float seconds, std::string category) {
        ::SharedTypes::v1_20_50::CooldownItemComponent payload{};
        payload.mDuration = seconds;
        payload.mCategory = std::move(category);
        payload.mType     = ::SharedTypes::ItemCooldownType::Use;
        return addComponent("minecraft:cooldown", std::make_shared<::CooldownItemComponent>(payload));
    }

    ICustomItem& useModifiers(float durationSeconds, std::optional<float> movementModifier = std::nullopt) {
        ::SharedTypes::Beta::UseModifiersItemComponent payload{};
        payload.mUseDuration      = durationSeconds;
        payload.mMovementModifier = movementModifier;
        payload.mEmitVibrations   = false;
        payload.mStartUsing       = ::SharedTypes::v1_26_30::UseModifiersItemComponent::StartUsing::Always;
        return addComponent("minecraft:use_modifiers", std::make_shared<::UseModifiersItemComponent>(payload));
    }

    [[nodiscard]] ::ComponentItem* getComponentItem() const noexcept { return mComponentItem.get(); }

    void initComponentItem(::cereal::ReflectionCtx const& ctx) {
        mComponentItem = std::make_unique<::ComponentItem>(this->mFullName->getString(), this->mId, ctx);
        for (auto const& [name, component] : mComponents) {
            if (!component) continue;
            component->mOwner = mComponentItem.get();
            mComponentItem->mItemComponents->emplace(::HashedString{name}, component);
            mComponentItem->mRegisteredCerealComponents->emplace(name, component);
            if (name == "minecraft:icon") {
                mComponentItem->mIcon = std::static_pointer_cast<::IconItemComponent>(component);
            }
            component->_initializeComponent(*mComponentItem);
        }

        mDescription->mIdentifier->mValue = this->mFullName->getString();
        this->mCreativeCategory           = static_cast<::SharedTypes::CreativeItemCategory>(
            static_cast<uchar>((*mDescription->mCategory).mCreativeCategory)
        );
        this->mCreativeGroup      = (*mDescription->mCategory).mCreativeGroupName->value_or("");
        this->mIsHiddenInCommands = (*mDescription->mCategory).mIsHiddenInCommands->value_or(false)
                                      ? ::ItemCommandVisibility::Hidden
                                      : ::ItemCommandVisibility::Visible;
    }

    std::unique_ptr<::CompoundTag> buildNetworkTag() const override {
        return mComponentItem ? mComponentItem->buildNetworkTag() : T::buildNetworkTag();
    }

    bool isComponentBased() const override { return false; }
};

} // namespace modapi::inline item
