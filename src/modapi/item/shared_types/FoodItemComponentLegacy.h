#pragma once
#include <mc/deps/core/math/Vec3.h>
#include <mc/deps/core/string/HashedString.h>
#include <mc/molang/MolangVersion.h>
#include <mc/world/item/Item.h>
#include <mc/world/item/components/IFoodItemComponent.h>

class FoodItemComponentLegacy : public ::IFoodItemComponent {
public:
    enum class OnUseAction : int {
        None                 = -1,
        ChorusTeleport       = 0,
        SuspiciousStewEffect = 1,
    };

    struct Effect {
    public:
        ll::UntypedStorage<4, 4>  mUnkd4071e;
        ll::UntypedStorage<8, 32> mUnk986c0d;
        ll::UntypedStorage<8, 32> mUnk12392a;
        ll::UntypedStorage<4, 4>  mUnk859cf2;
        ll::UntypedStorage<4, 4>  mUnk7b370d;
        ll::UntypedStorage<4, 4>  mUnk7abd7a;

    public:
        Effect& operator=(Effect const&);
        Effect(Effect const&);
        Effect();

    public:
        MCAPI ~Effect();
    };

public:
    Item&                 mOwner;
    int                   mNutrition;
    float                 mSaturationModifier;
    std::string           mUsingConvertTo;
    OnUseAction           mOnUseAction;
    Vec3                  mOnUseRange;
    ::HashedString        mCooldownType;
    int                   mCooldownTime;
    bool                  mCanAlwaysEat;
    std::vector<Effect>   mEffects;
    std::vector<uint32_t> mRemoveEffects;

public:
    FoodItemComponentLegacy& operator=(FoodItemComponentLegacy const&);
    FoodItemComponentLegacy(FoodItemComponentLegacy const&);

    explicit FoodItemComponentLegacy(Item& owner) : mOwner(owner) {}

public:
    virtual int getNutrition() const;

    virtual float getSaturationModifier() const;

    virtual bool canAlwaysEat() const;

    virtual ::Item const* eatItem(::ItemStack& instance, ::Actor& actor, ::Level& level);

    virtual void use(bool& result, ::ItemStack& item, ::Player& player);

    virtual ::Item const* useTimeDepleted(
        ::ItemUseMethod& itemUseMethod,
        ::ItemStack const&,
        ::ItemStack& instance,
        ::Player&    player,
        ::Level&     level
    );

    virtual ~FoodItemComponentLegacy();

public:
    MCAPI void _applyEatEffects(::ItemStack const& actor, ::Actor& level, ::Level&);

    MCAPI void _loadEffects(::Json::Value const& effectsData);

    MCAPI void _loadRemoveEffects(::Json::Value const& removeEffectsData);

    MCAPI ::std::unique_ptr<::CompoundTag> buildNetworkTag() const;

    MCAPI bool init(::Json::Value const& data, ::MolangVersion);

public:
    MCAPI static ::Json::Value initializeFromNetwork(::CompoundTag const& tag);
};
