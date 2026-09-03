#include "modapi/item/shared_types/FoodItemComponentLegacy.h"

#include <mc/deps/nbt/CompoundTag.h>
#include <mc/deps/nbt/FloatTag.h>
#include <mc/deps/nbt/IntTag.h>
#include <mc/deps/nbt/ListTag.h>
#include <mc/world/item/query/ItemOnUseResult.h>

FoodItemComponentLegacy::Effect& FoodItemComponentLegacy::Effect::operator=(Effect const&) = default;
FoodItemComponentLegacy::Effect::Effect(Effect const&)                                     = default;
FoodItemComponentLegacy::Effect::Effect()                                                  = default;
FoodItemComponentLegacy::Effect::~Effect()                                                 = default;

int FoodItemComponentLegacy::getNutrition() const { return mNutrition; }

float FoodItemComponentLegacy::getSaturationModifier() const { return mSaturationModifier; }

bool FoodItemComponentLegacy::canAlwaysEat() const { return mCanAlwaysEat; }

::Item const* FoodItemComponentLegacy::eatItem(::ItemStack&, ::Actor&, ::Level&) { return nullptr; }

void FoodItemComponentLegacy::use(::ItemOnUseResult&, ::ItemStack&, ::Player&) {}

::Item const*
FoodItemComponentLegacy::useTimeDepleted(::ItemUseMethod&, ::ItemStack const&, ::ItemStack&, ::Player&, ::Level&) {
    return nullptr;
}

FoodItemComponentLegacy::~FoodItemComponentLegacy() = default;

::std::unique_ptr<::CompoundTag> FoodItemComponentLegacy::buildNetworkTag() const {
    auto tag = std::make_unique<::CompoundTag>();
    tag->putInt("nutrition", mNutrition);
    tag->putFloat("saturation_modifier", mSaturationModifier);
    tag->putBoolean("can_always_eat", mCanAlwaysEat);
    tag->putString("cooldown_type", mCooldownType.getString());
    tag->putInt("cooldown_time", mCooldownTime);
    tag->putString("using_converts_to", mUsingConvertTo);
    tag->putInt("on_use_action", (int)mOnUseAction);

    auto range = std::make_unique<::ListTag>();
    range->add(std::make_unique<::FloatTag>(mOnUseRange.x));
    range->add(std::make_unique<::FloatTag>(mOnUseRange.y));
    range->add(std::make_unique<::FloatTag>(mOnUseRange.z));
    tag->put("on_use_range", std::move(range));

    if (!mRemoveEffects.empty()) {
        auto removeEffects = std::make_unique<::ListTag>();
        for (auto id : mRemoveEffects) {
            removeEffects->add(std::make_unique<::IntTag>((int)id));
        }
        tag->put("remove_effects", std::move(removeEffects));
    }
    return tag;
}
