#include "modapi/recipe/base/ICustomFurnaceRecipe.h"
#include "modapi/recipe/SharedTypes.h"

namespace modapi::inline recipe {

ICustomFurnaceRecipe::ICustomFurnaceRecipe() = default;

std::string ICustomFurnaceRecipe::getRecipeId() const { return {}; }

std::vector<::std::string> ICustomFurnaceRecipe::getCraftingTags() const { return {"furnace"}; }

CustomFurnaceRecipeBase::CustomFurnaceRecipeBase(
    ICustomRecipe::Ingredient const& input,
    ::ItemInstance const&            output,
    std::vector<std::string> const&  craftingTags
)
: mOutput(output) {
    // `getId()`/`getAuxValue()` are only meaningful once the item is resolved, so the input is built
    // from its name. Constructing it from a bare `ItemStackBase` left both unresolved, which put the
    // recipe into the furnace table under the wrong key.
    mInput.reinit(input.pImpl->mType, input.pImpl->mCount, input.pImpl->mAux);
    for (auto& tag : craftingTags) {
        mTags.emplace_back(tag);
    }
}

void CustomFurnaceRecipeBase::registerRecipe(::Recipes& registry) {
    (*registry.mFurnaceResults)[mInput.getId()][mInput.getAuxValue()] = mOutput;
}

void ICustomFurnaceRecipe::_init() {
    pImpl->mFurnaceRecipe.emplace(CustomFurnaceRecipeBase(getIngredient(), getResult(), getCraftingTags()));
}

} // namespace modapi::inline recipe