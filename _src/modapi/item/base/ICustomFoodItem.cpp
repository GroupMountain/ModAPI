#include "gmlib/mod/item/shared_types/FoodItemComponentLegacy.h"
#include <gmlib/mod/item/base/ICustomFoodItem.h>
#include <mc/deps/core/math/Vec3.h>
#include <mc/world/actor/player/Player.h>
#include <mc/world/effect/EffectDuration.h>
#include <mc/world/level/Level.h>

namespace gmlib::mod {

ICustomFoodItem::ICustomFoodItem(std::string const& identifier) : ICustomItem(identifier) {}

bool ICustomFoodItem::canAlwaysEat() const { return false; }

std::string ICustomFoodItem::getUsingConvertTo() const { return {}; }

ICustomFoodItem::UseAction ICustomFoodItem::getUseAction() const { return ICustomFoodItem::UseAction::None; }

Vec3 ICustomFoodItem::getOnUseRange() const { return Vec3::ZERO(); }

std::vector<::MobEffectInstance> ICustomFoodItem::getEffects() const { return {}; }

std::vector<uint32_t> ICustomFoodItem::getRemoveEffects() const { return {}; }

bool ICustomFoodItem::isFood() const { return true; }

::SharedTypes::Legacy::UseAnimation ICustomFoodItem::getUseAnimation() const {
    return ::SharedTypes::Legacy::UseAnimation::Eat;
}

::ItemUseMethod ICustomFoodItem::useTimeDepleted(::ItemStack& inoutInstance, ::Level* level, ::Player* player) const {
    auto method  = ICustomItem::useTimeDepleted(inoutInstance, level, player);
    auto effects = getEffects();
    for (auto& effect : effects) {
        player->addEffect(effect);
    }
    return method;
}

void ICustomFoodItem::_init() {
    ICustomItem::_init();
    mFoodComponentLegacy                      = std::make_unique<FoodItemComponentLegacy>(*this);
    mFoodComponentLegacy->mNutrition          = getNutrition();
    mFoodComponentLegacy->mSaturationModifier = getSaturation();
    mFoodComponentLegacy->mCanAlwaysEat       = canAlwaysEat();
    mFoodComponentLegacy->mUsingConvertTo     = getUsingConvertTo();
    mFoodComponentLegacy->mOnUseAction        = FoodItemComponentLegacy::OnUseAction((int)getUseAction());
    mFoodComponentLegacy->mOnUseRange         = getOnUseRange();
    mFoodComponentLegacy->mRemoveEffects      = getRemoveEffects();
}

::MobEffectInstance ICustomFoodItem::createEffect(
    MobEffectType effectType,
    int           durationTicks,
    int           amplifier,
    bool          visible,
    bool          ambient,
    bool          animation
) {
    auto result                             = ::MobEffectInstance((int)effectType);
    result.mDuration                        = EffectDuration(durationTicks);
    result.mAmplifier                       = amplifier;
    result.mEffectVisible                   = visible;
    result.mAmbient                         = ambient;
    result.mDisplayOnScreenTextureAnimation = animation;
    return std::move(result);
}

} // namespace gmlib::mod