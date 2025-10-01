#include "gmlib/mod/recipe/SharedTypes.h"
#include <gmlib/mod/recipe/base/ICustomShapelessRecipe.h>
#include <mc/world/item/crafting/Recipe.h>

namespace gmlib::mod {

CustomShapelessRecipeBase::CustomShapelessRecipeBase(
    std::string_view                             craftingTag,
    std::string_view                             recipeId,
    std::vector<ICustomRecipe::Ingredient> const ingredients,
    ::ItemInstance const&                        result,
    int                                          priority,
    ICustomRecipe::UnlockingRequirement const    unlock
)
: ShapelessRecipe() {
    mTag      = HashedString(craftingTag);
    mRecipeId = recipeId;
    mWidth    = 0;
    mHeight   = 0;
    for (auto& ingredient : ingredients) {
        auto inputItem = ingredient.pImpl->serialize();
        for (ushort i = 0; i < inputItem.mStackSize; i++) {
            mMyIngredients->push_back(inputItem);
        }
    }
    mResults->mResultsAreLoaded = true;
    mResults->mResults->push_back(result);
    mPriority             = priority;
    mUnlockingRequirement = unlock.pImpl->serialize();
}

ICustomShapelessRecipe::ICustomShapelessRecipe() = default;

void ICustomShapelessRecipe::_init() {
    auto tags = getCraftingTags();
    pImpl->mRecipes.reserve(tags.size());
    for (auto const& tag : tags) {
        auto recipe = std::make_unique<CustomShapelessRecipeBase>(
            tag,
            getRecipeId(),
            getIngredients(),
            getResult(),
            getPriority(),
            getUnlockingRequirement()
        );
        pImpl->mRecipes.push_back(std::move(recipe));
    }
}

std::vector<::std::string> ICustomShapelessRecipe::getCraftingTags() const { return {"crafting_table"}; }

int ICustomShapelessRecipe::getPriority() const { return 50; }

ICustomRecipe::UnlockingRequirement ICustomShapelessRecipe::getUnlockingRequirement() const {
    return ICustomRecipe::UnlockingRequirement(RecipeUnlockingContext::AlwaysUnlocked);
}

} // namespace gmlib::mod