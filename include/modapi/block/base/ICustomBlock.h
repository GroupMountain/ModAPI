#pragma once
#include "modapi/Macros.h"
#include <mc/deps/core/math/Vec3.h>
#include <mc/deps/core/string/HashedString.h>
#include <mc/world/level/block/components/BlockCollisionBoxDescription.h>
#include <mc/world/level/block/components/BlockDestructibleByMiningDescription.h>
#include <mc/world/level/block/components/BlockDisplayNameDescription.h>
#include <mc/world/level/block/components/BlockFrictionDescription.h>
#include <mc/world/level/block/components/BlockLightEmissionDescription.h>
#include <mc/world/level/block/components/BlockMapColorDescription.h>
#include <mc/world/level/block/components/BlockSelectionBoxDescription.h>
#include <mc/world/level/block/definition/BlockComponentGroupDescription.h>
#include <mc/world/level/block/definition/BlockDescription.h>
#include <mc/world/level/block/definition/BlockPermutationDescription.h>
#include <mc/world/level/block/definition/BlockStateDefinition.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace modapi::inline block {

class BlockRegistry;

// Block description in the engine's own types.
struct BlockProperty {
    ::BlockComponentGroupDescription             mComponents;
    ::BlockDescription                           mDescription;
    ::std::vector<::BlockPermutationDescription> mPermutations;
    int                                          mMolangVersion = 12;
};

template <class T>
class ICustomBlock : public T {
    BlockProperty mProperty;
    friend class BlockRegistry;

public:
    using BlockBase = T;
    using T::T;

    ~ICustomBlock() override = default;

    [[nodiscard]] ::BlockComponentGroupDescription const& components() const noexcept { return mProperty.mComponents; }

    ICustomBlock& components(::BlockComponentGroupDescription value) {
        mProperty.mComponents = std::move(value);
        return *this;
    }

    ICustomBlock& addComponent(std::string name, std::shared_ptr<::BlockComponentDescription> component) {
        mProperty.mComponents.mCerealDescriptions->mMap->emplace(std::move(name), std::move(component));
        return *this;
    }

    [[nodiscard]] std::vector<std::string> blockTags() const {
        std::vector<std::string> result;
        result.reserve(mProperty.mComponents.mTags->size());
        for (auto const& tag : *mProperty.mComponents.mTags) result.push_back(tag.getString());
        return result;
    }

    ICustomBlock& blockTags(std::vector<std::string> value) {
        mProperty.mComponents.mTags->clear();
        for (auto const& tag : value) mProperty.mComponents.mTags->push_back(::HashedString{tag});
        return *this;
    }

    ICustomBlock& collisionBox(::Vec3 const& origin, ::Vec3 const& size) {
        return addComponent("minecraft:collision_box", std::make_shared<::BlockCollisionBoxDescription>(origin, size));
    }

    ICustomBlock& selectionBox(::Vec3 const& origin, ::Vec3 const& size) {
        auto component      = std::make_shared<::BlockSelectionBoxDescription>();
        component->mEnabled = true;
        component->mOrigin  = origin;
        component->mSize    = size;
        return addComponent("minecraft:selection_box", std::move(component));
    }

    ICustomBlock& destructibleByMining(float seconds) {
        return addComponent(
            "minecraft:destructible_by_mining",
            std::make_shared<::BlockDestructibleByMiningDescription>(seconds)
        );
    }

    ICustomBlock& lightEmission(::Brightness value) {
        auto component            = std::make_shared<::BlockLightEmissionDescription>();
        component->mLightEmission = value;
        return addComponent("minecraft:light_emission", std::move(component));
    }

    ICustomBlock& displayName(std::string value) {
        auto component          = std::make_shared<::BlockDisplayNameDescription>();
        component->mDisplayName = std::move(value);
        return addComponent("minecraft:display_name", std::move(component));
    }

    ICustomBlock& mapColor(::SharedTypes::Color255RGB value, ::TintMethod tint) {
        auto component         = std::make_shared<::BlockMapColorDescription>();
        component->mMapColor   = value;
        component->mTintMethod = tint;
        return addComponent("minecraft:map_color", std::move(component));
    }

    ICustomBlock& friction(float value) {
        auto component       = std::make_shared<::BlockFrictionDescription>();
        component->mFriction = value;
        return addComponent("minecraft:friction", std::move(component));
    }

    [[nodiscard]] ::BlockDescription const& description() const noexcept { return mProperty.mDescription; }

    ICustomBlock& description(::BlockDescription value) {
        mProperty.mDescription = std::move(value);
        return *this;
    }

    [[nodiscard]] ::BlockMenuCategory const& menuCategory() const noexcept {
        return *mProperty.mDescription.mMenuCategory;
    }

    ICustomBlock& menuCategory(::BlockMenuCategory value) {
        *mProperty.mDescription.mMenuCategory = std::move(value);
        return *this;
    }

    [[nodiscard]] ::SharedTypes::v1_26_20::BlockDefinition::VanillaBlockData const& vanillaBlockData() const noexcept {
        return *mProperty.mDescription.mVanillaBlockData;
    }

    ICustomBlock& vanillaBlockData(::SharedTypes::v1_26_20::BlockDefinition::VanillaBlockData value) {
        *mProperty.mDescription.mVanillaBlockData = std::move(value);
        return *this;
    }

    [[nodiscard]] std::vector<::BlockStateDefinition> const& states() const noexcept {
        return *mProperty.mDescription.mStates;
    }

    ICustomBlock& states(std::vector<::BlockStateDefinition> value) {
        *mProperty.mDescription.mStates = std::move(value);
        return *this;
    }

    [[nodiscard]] std::vector<::BlockPermutationDescription> const& permutations() const noexcept {
        return mProperty.mPermutations;
    }

    ICustomBlock& permutations(std::vector<::BlockPermutationDescription> value) {
        mProperty.mPermutations = std::move(value);
        return *this;
    }

    [[nodiscard]] int molangVersion() const noexcept { return mProperty.mMolangVersion; }

    ICustomBlock& molangVersion(int value) {
        mProperty.mMolangVersion = value;
        return *this;
    }

    [[nodiscard]] BlockProperty const& getProperty() const noexcept { return mProperty; }
};

} // namespace modapi::inline block
