#include "modapi/recipe/base/ICustomShapedMultiRecipe.h"
#include "modapi/recipe/SharedTypes.h"
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

namespace modapi::inline recipe {

ICustomShapedMultiRecipe::ICustomShapedMultiRecipe() = default;

CustomShapedMultiRecipeBase::CustomShapedMultiRecipeBase(
    std::string_view                             craftingTag,
    std::string_view                             recipeId,
    std::vector<std::string> const&              shape,
    ICustomShapedRecipe::ShapedIngredients const ingredients,
    ::ItemInstance const&                        result,
    int                                          priority,
    ICustomRecipe::UnlockingRequirement const    unlock,
    bool                                         assumeSymmetry,
    std::function<void(std::unordered_map<std::string, std::vector<::ItemStack const*>>, ::ItemInstance&)>&&
        craftingFunction
)
: CustomShapedRecipeBase(craftingTag, recipeId, shape, ingredients, result, priority, unlock, assumeSymmetry),
  mCraftingFunction(std::move(craftingFunction)),
  mIngredients(ingredients) {
    mCraftingResult.push_back(result);
    if (!mCraftingFunction) {
        mCraftingFunction = [](std::unordered_map<std::string, std::vector<::ItemStack const*>>, ::ItemInstance&) {};
    }
}

::std::vector<::ItemInstance> const&
CustomShapedMultiRecipeBase::assemble(::CraftingContainer& container, ::CraftingContext&) const {
    mRuntimeResult = mCraftingResult;
    if (mRuntimeResult.size() == 1) {
        std::unordered_map<std::string, std::vector<::ItemStack const*>> inputInfo;
        for (auto& [key, ingredient] : mIngredients.mIngredients) {
            for (auto& item : *container.mItems) {
                if (!item.isNull()) {
                    auto ingredientItem = ingredient.pImpl->serialize();
                    if (ingredientItem.sameItem(
                            ItemDescriptor(*item.getItem(), item.getAuxValue()),
                            (ingredientItem.getAuxValue() != 32767)
                        )) {
                        inputInfo[key].push_back(&item);
                    }
                }
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

::std::vector<::ItemInstance> const& CustomShapedMultiRecipeBase::getResultItems() const { return mCraftingResult; }

void ICustomShapedMultiRecipe::_init() {
    auto tags = getCraftingTags();
    pImpl->mRecipes.reserve(tags.size());
    for (auto const& tag : tags) {
        auto recipe = std::make_unique<CustomShapedMultiRecipeBase>(
            tag,
            getRecipeId(),
            getShape(),
            getIngredients(),
            getResult(),
            getPriority(),
            getUnlockingRequirement(),
            isAssumeSymmetry(),
            getCraftingCallback()
        );
        pImpl->mRecipes.push_back(std::move(recipe));
    }
}

} // namespace modapi::inline recipe
