#include "gmlib/mod/recipe/SharedTypes.h"
#include <gmlib/mod/recipe/base/ICustomShapelessMultiRecipe.h>
#include <ll/api/chrono/GameChrono.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/thread/ServerThreadExecutor.h>
#include <mc/common/SharedConstants.h>
#include <mc/deps/core/sem_ver/SemVersion.h>
#include <mc/world/actor/player/Player.h>
#include <mc/world/inventory/CraftingContainer.h>
#include <mc/world/item/NetworkItemInstanceDescriptor.h>
#include <mc/world/item/crafting/CraftingContext.h>
#include <mc/world/item/crafting/Recipe.h>
#include <mc/world/level/Level.h>

namespace gmlib::mod {

ICustomShapelessMultiRecipe::ICustomShapelessMultiRecipe() = default;

CustomShapelessMultiRecipeBase::CustomShapelessMultiRecipeBase(
    std::string_view                                                        craftingTag,
    std::string_view                                                        recipeId,
    std::vector<ICustomRecipe::Ingredient> const                            ingredients,
    ::ItemInstance const&                                                   result,
    int                                                                     priority,
    ICustomRecipe::UnlockingRequirement const                               unlock,
    std::function<void(std::vector<::ItemStack const*>, ::ItemInstance&)>&& craftingFunction
)
: CustomShapelessRecipeBase(craftingTag, recipeId, ingredients, result, priority, unlock),
  mCraftingFunction(std::move(craftingFunction)) {
    mCraftingResult.push_back(result);
    if (!mCraftingFunction) {
        mCraftingFunction = [](std::vector<::ItemStack const*>, ::ItemInstance&) {};
    }
}

::std::vector<::ItemInstance> const&
CustomShapelessMultiRecipeBase::assemble(::CraftingContainer& container, ::CraftingContext&) const {
    mRuntimeResult = mCraftingResult;
    if (mRuntimeResult.size() == 1) {
        std::vector<::ItemStack const*> inputInfo;
        for (auto& item : *container.mItems) {
            if (!item.isNull()) {
                inputInfo.push_back(&item);
            }
        }
        try {
            mCraftingFunction(inputInfo, mRuntimeResult.at(0));
        } catch (...) {}
        ll::thread::ServerThreadExecutor::getDefault().executeAfter(
            [] {
                ll::service::getLevel()->forEachPlayer([](const Player& pl) -> bool {
                    const_cast<Player&>(pl).refreshInventory();
                    return true;
                });
            },
            ll::chrono::ticks(1)
        );
    }
    return mRuntimeResult;
}

::std::vector<::ItemInstance> const& CustomShapelessMultiRecipeBase::getResultItems() const { return mCraftingResult; }

void ICustomShapelessMultiRecipe::_init() {
    auto tags = getCraftingTags();
    pImpl->mRecipes.reserve(tags.size());
    for (auto const& tag : tags) {
        auto recipe = std::make_unique<CustomShapelessMultiRecipeBase>(
            tag,
            getRecipeId(),
            getIngredients(),
            getResult(),
            getPriority(),
            getUnlockingRequirement(),
            getCraftingCallback()
        );
        pImpl->mRecipes.push_back(std::move(recipe));
    }
}

} // namespace gmlib::mod
