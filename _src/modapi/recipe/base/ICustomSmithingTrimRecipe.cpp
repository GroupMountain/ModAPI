#include "gmlib/mod/recipe/SharedTypes.h"
#include <gmlib/mod/recipe/base/ICustomSmithingTrimRecipe.h>

namespace gmlib::mod {

ICustomSmithingTrimRecipe::ICustomSmithingTrimRecipe() = default;

void ICustomSmithingTrimRecipe::_init() {
    auto tags = getCraftingTags();
    pImpl->mRecipes.reserve(tags.size());
    for (auto const& tag : tags) {
        auto recipe = std::make_unique<::SmithingTrimRecipe>(
            getRecipeId(),
            getSmithingTemplate().pImpl->serialize(),
            getBaseIngredient().pImpl->serialize(),
            getAdditionIngredient().pImpl->serialize(),
            HashedString(tag)
        );
        pImpl->mRecipes.push_back(std::move(recipe));
    }
}

std::vector<::std::string> ICustomSmithingTrimRecipe::getCraftingTags() const { return {"smithing_table"}; }

} // namespace gmlib::mod