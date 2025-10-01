#include "gmlib/mod/item/shared_types/ItemInitializer.h"
#include <mc/world/level/block/ComposterBlock.h>

namespace gmlib::mod {

void initCustomItem(ICustomItem& item) {
    item.mMaxStackSize = item.getItemMaxStackSize();
    auto tags          = item.getItemTags();
    for (auto& tag : tags) {
        item.addTag(ItemTag(tag));
    }
    item.mAllowOffhand         = item.allowOffhand();
    item.mHoverTextColorFormat = item.getHoverTextColorFormat();
    item.mBaseRarity           = item.getBaseRarity();
    item.mShouldDespawn        = item.shouldDespawn();
    item.mIsGlint              = item.isFoil();
    item.mUseAnim              = item.getUseAnimation();
    item.mIsStackedByData      = item.isStackedByData();
    item.mRequiresWorldBuilder = item.requiresWorldBuilder();
    item.mExplodable           = item.isExplodable();
    item.mFireResistant        = item.isFireResistant();
    item.mIgnoresPermissions   = item.shouldIgnoresPermissions();
    item.mAnimatesInToolbar    = item.shouldAnimatesInToolbar();
    item.mMaxDamage            = item.getMaxDamage();
    item.mHandEquipped         = item.isHandEquipped();
    item.mMaxUseDuration       = item.getUseDuration();
    item.mMineBlockType        = item.getMineBlockItemEffectType();
    item.mCreativeCategory     = item.getCreativeCategory();
    item.mCreativeGroup        = item.getCreativeGroup();
    if (item.isFuel()) {
        item.mFurnaceBurnIntervalModifier = item.getFurnaceBurnInterval();
        if (item.mFurnaceBurnIntervalModifier < 0.05f) {
            item.mFurnaceBurnIntervalModifier = 0.05f;
        }
    }
    item.mFurnaceXPmultiplier = item.getFurnaceXPmultiplier();
    item.mIsHiddenInCommands  = item.shouldHiddenInCommands();
    item.mFrameCount          = item.getFrameCount();
    if (item.isSmithingTemplate()) {
        item.addTag("minecraft:transform_templates");
    }
    if (item.isSmithingTransformable()) {
        item.addTag("minecraft:transformable_items");
    }
    if (item.isSmithingTransformMaterial()) {
        item.addTag("minecraft:transform_materials");
    }
    if (item.isFood()) {
        item.addTag("minecraft:is_food");
    }
    (void)(item.pImpl->mCooldownCategory = ::HashedString(item.getCooldownCategory()));
    if (item.getCompostChance() > 0) {
        auto chance = item.getCompostChance();
        if (chance > 100) chance = 100;
        const_cast<std::unordered_map<uint64, int8_t>&>(ComposterBlock::_getCompostableItems()
        )[item.mFullName->mStrHash] = chance;
    }
}

void initCustomItem(ICustomArmorItem& item) {
    item.mMaxStackSize = item.getItemMaxStackSize();
    auto tags          = item.getItemTags();
    for (auto& tag : tags) {
        item.addTag(ItemTag(tag));
    }
    item.mAllowOffhand         = item.allowOffhand();
    item.mHoverTextColorFormat = item.getHoverTextColorFormat();
    item.mBaseRarity           = item.getBaseRarity();
    item.mShouldDespawn        = item.shouldDespawn();
    item.mIsGlint              = item.isFoil();
    item.mUseAnim              = item.getUseAnimation();
    item.mIsStackedByData      = item.isStackedByData();
    item.mRequiresWorldBuilder = item.requiresWorldBuilder();
    item.mExplodable           = item.isExplodable();
    item.mFireResistant        = item.isFireResistant();
    item.mIgnoresPermissions   = item.shouldIgnoresPermissions();
    item.mAnimatesInToolbar    = item.shouldAnimatesInToolbar();
    item.mMaxDamage            = item.getMaxDamage();
    item.mHandEquipped         = item.isHandEquipped();
    item.mMaxUseDuration       = item.getUseDuration();
    item.mMineBlockType        = item.getMineBlockItemEffectType();
    item.mCreativeCategory     = item.getCreativeCategory();
    item.mCreativeGroup        = item.getCreativeGroup();
    if (item.isFuel()) {
        item.mFurnaceBurnIntervalModifier = item.getFurnaceBurnInterval();
        if (item.mFurnaceBurnIntervalModifier < 0.05f) {
            item.mFurnaceBurnIntervalModifier = 0.05f;
        }
    }
    item.mFurnaceXPmultiplier = item.getFurnaceXPmultiplier();
    item.mIsHiddenInCommands  = item.shouldHiddenInCommands();
    item.mFrameCount          = item.getFrameCount();
    if (item.isSmithingTemplate()) {
        item.addTag("minecraft:transform_templates");
    }
    if (item.isSmithingTransformable()) {
        item.addTag("minecraft:transformable_items");
    }
    if (item.isSmithingTransformMaterial()) {
        item.addTag("minecraft:transform_materials");
    }
    if (item.isFood()) {
        item.addTag("minecraft:is_food");
    }
    item.mSlot                 = item.getArmorSlot();
    item.mDefense              = item.getArmorValue();
    item.mModelIndex           = item.getModelIndex();
    item.getItemDamageChance() = item.getItemDamageChance();
    if (item.isTrimAllowed()) {
        item.addTag("minecraft:trimmable_armors");
    }
    item.getRepairItems() = item.getRepairItems();
    (void)(item.pImpl->mCooldownCategory = ::HashedString(item.getCooldownCategory()));
    if (item.getCompostChance() > 0) {
        auto chance = item.getCompostChance();
        if (chance > 100) chance = 100;
        const_cast<std::unordered_map<uint64, int8_t>&>(ComposterBlock::_getCompostableItems()
        )[item.mFullName->mStrHash] = chance;
    }
}

} // namespace gmlib::mod