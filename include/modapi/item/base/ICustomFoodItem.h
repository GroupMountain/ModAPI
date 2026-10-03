#pragma once
#include "modapi/effect/enum/MobEffectType.h"
#include "modapi/item/base/ICustomItem.h"
#include "modapi/item/shared_types/FoodItemComponentLegacy.h"
#include <mc/deps/core/math/Vec3.h>
#include <mc/world/actor/player/Player.h>
#include <mc/world/effect/EffectDuration.h>
#include <mc/world/effect/MobEffectInstance.h>
#include <mc/world/level/Level.h>

namespace modapi::inline item {

// A food item, on top of whatever engine base `T` is (`::Item` in the ordinary case). Everything it answers is written
// here rather than in a `.cpp`: a template member's definition has to be visible wherever `T` is instantiated, and its
// mangled name moves with `T`.
template <typename T>
class ICustomFoodItem : public ICustomItem<T> {
public:
    enum class UseAction : int {
        None                 = -1,
        ChorusTeleport       = 0,
        SuspiciousStewEffect = 1,
    };

    explicit ICustomFoodItem(std::string const& identifier) : ICustomItem<T>(identifier) {}

    virtual int getNutrition() const = 0;

    virtual float getSaturation() const = 0;

    virtual bool canAlwaysEat() const { return false; }

    virtual std::string getUsingConvertTo() const { return {}; }

    virtual UseAction getUseAction() const { return UseAction::None; }

    virtual Vec3 getOnUseRange() const { return Vec3::ZERO(); }

    virtual std::vector<::MobEffectInstance> getEffects() const { return {}; }

    virtual std::vector<uint32_t> getRemoveEffects() const { return {}; }

    bool isFood() const override { return true; }

    ::SharedTypes::Legacy::UseAnimation getUseAnimation() const override {
        return ::SharedTypes::Legacy::UseAnimation::Eat;
    }

    ::ItemUseMethod useTimeDepleted(::ItemStack& inoutInstance, ::Level* level, ::Player* player) const override {
        auto method  = T::useTimeDepleted(inoutInstance, level, player);
        auto effects = getEffects();
        for (auto& effect : effects) {
            player->addEffect(effect);
        }
        return method;
    }

    void _init() {
        ICustomItem<T>::_init();
        this->mFoodComponentLegacy                      = std::make_unique<FoodItemComponentLegacy>(*this);
        this->mFoodComponentLegacy->mNutrition          = getNutrition();
        this->mFoodComponentLegacy->mSaturationModifier = getSaturation();
        this->mFoodComponentLegacy->mCanAlwaysEat       = canAlwaysEat();
        this->mFoodComponentLegacy->mUsingConvertTo     = getUsingConvertTo();
        this->mFoodComponentLegacy->mOnUseAction        = FoodItemComponentLegacy::OnUseAction((int)getUseAction());
        this->mFoodComponentLegacy->mOnUseRange         = getOnUseRange();
        this->mFoodComponentLegacy->mRemoveEffects      = getRemoveEffects();
    }

    static ::MobEffectInstance createEffect(
        MobEffectType effectType,
        int           durationTicks = 600,
        int           amplifier     = 0,
        bool          visible       = true,
        bool          ambient       = false,
        bool          animation     = false
    ) {
        auto result                             = ::MobEffectInstance((uint)effectType, EffectDuration(durationTicks));
        result.mDuration                        = EffectDuration(durationTicks);
        result.mAmplifier                       = amplifier;
        result.mEffectVisible                   = visible;
        result.mAmbient                         = ambient;
        result.mDisplayOnScreenTextureAnimation = animation;
        return result;
    }
};

} // namespace modapi::inline item
