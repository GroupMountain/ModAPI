#include "modapi/recipe/base/ICustomSmithingTransformRecipe.h"
#include "modapi/recipe/SharedTypes.h"

namespace modapi::inline recipe {

ICustomSmithingTransformRecipe::ICustomSmithingTransformRecipe() = default;

void ICustomSmithingTransformRecipe::_init() {
    auto tags   = getCraftingTags();
    auto result = getResult();
    pImpl->mRecipes.reserve(tags.size());
    for (auto const& tag : tags) {
        pImpl->mRecipes.push_back(
            makeCustomSmithingRecipe<::SmithingTransformRecipe>(
                tag,
                getRecipeId(),
                getSmithingTemplate(),
                getBaseIngredient(),
                getAdditionIngredient(),
                &result
            )
        );
    }
}

std::vector<::std::string> ICustomSmithingTransformRecipe::getCraftingTags() const { return {"smithing_table"}; }

} // namespace modapi::inline recipe