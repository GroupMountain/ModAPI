#include "gmlib/mod/recipe/SharedTypes.h"
#include <gmlib/mod/recipe/base/ICustomSmithingTransformRecipe.h>

namespace gmlib::mod {

ICustomSmithingTransformRecipe::ICustomSmithingTransformRecipe() = default;

void ICustomSmithingTransformRecipe::_init() {
    auto tags = getCraftingTags();
    pImpl->mRecipes.reserve(tags.size());
    for (auto const& tag : tags) {
        ::Recipe::Results recipeResult;
        recipeResult.mResultsAreLoaded = true;
        recipeResult.mResults->push_back(getResult());
        auto recipe = std::make_unique<::SmithingTransformRecipe>(
            getRecipeId(),
            getSmithingTemplate().pImpl->serialize(),
            getBaseIngredient().pImpl->serialize(),
            getAdditionIngredient().pImpl->serialize(),
            std::move(recipeResult),
            HashedString(tag)
        );
        pImpl->mRecipes.push_back(std::move(recipe));
    }
}

std::vector<::std::string> ICustomSmithingTransformRecipe::getCraftingTags() const { return {"smithing_table"}; }

} // namespace gmlib::mod