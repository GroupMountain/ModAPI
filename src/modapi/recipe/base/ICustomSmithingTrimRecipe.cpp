#include "modapi/recipe/base/ICustomSmithingTrimRecipe.h"
#include "modapi/recipe/SharedTypes.h"

namespace modapi::inline recipe {

ICustomSmithingTrimRecipe::ICustomSmithingTrimRecipe() = default;

void ICustomSmithingTrimRecipe::_init() {
    auto tags = getCraftingTags();
    pImpl->mRecipes.reserve(tags.size());
    for (auto const& tag : tags) {
        pImpl->mRecipes.push_back(
            makeCustomSmithingRecipe<::SmithingTrimRecipe>(
                tag,
                getRecipeId(),
                getSmithingTemplate(),
                getBaseIngredient(),
                getAdditionIngredient()
            )
        );
    }
}

std::vector<::std::string> ICustomSmithingTrimRecipe::getCraftingTags() const { return {"smithing_table"}; }

} // namespace modapi::inline recipe