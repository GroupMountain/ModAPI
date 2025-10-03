#include "modapi/recipe/base/ICustomBrewingRecipe.h"
#include "modapi/recipe/SharedTypes.h"
#include <mc/world/item/Item.h>


namespace modapi::inline recipe {

ICustomBrewingRecipe::ICustomBrewingRecipe() = default;

std::string ICustomBrewingRecipe::getRecipeId() const { return {}; }

std::vector<::std::string> ICustomBrewingRecipe::getCraftingTags() const { return {"brewing_stand"}; }

CustomBrewingRecipeBase::CustomBrewingRecipeBase(
    ICustomRecipe::Ingredient const& input,
    ICustomRecipe::Ingredient const& reagent,
    ICustomRecipe::Ingredient const& output
)
: mInput(input),
  mReagent(reagent),
  mOutput(output) {
    if (mOutput.pImpl->mAux == 32767) {
        mOutput.pImpl->mAux = 0;
    }
}

void CustomBrewingRecipeBase::registerRecipe() {
    if (mInput.pImpl->mAux == 32767 && mOutput.pImpl->mAux == 0) {
        PotionBrewing::addContainerRecipe(
            *ItemInstance(mInput.pImpl->mType).getItem(),
            PotionBrewing::Ingredient(ItemInstance(mReagent.pImpl->mType).getId(), mReagent.pImpl->mAux),
            *ItemInstance(mOutput.pImpl->mType).getItem()
        );
    } else {
        if (mReagent.pImpl->mAux == 32767) {
            mReagent.pImpl->mAux = 0;
        }
        for (auto& recipe : PotionBrewing::mPotionMixes()) {
            if (recipe.mFrom.sameItem(mInput.pImpl->serialize(), true)
                && recipe.mIngredient.mItemId == ::RecipeIngredient(mReagent.pImpl->mType, 0, 1).getId()
                && recipe.mIngredient.mData == mReagent.pImpl->mAux) {
                recipe.mTo = ::RecipeIngredient(mOutput.pImpl->mType, 0, 1);
                return;
            }
        }
        PotionBrewing::addPotionMix(
            ItemDescriptor(mInput.pImpl->mType, mInput.pImpl->mAux),
            PotionBrewing::Ingredient(ItemInstance(mReagent.pImpl->mType).getId(), mReagent.pImpl->mAux),
            ItemDescriptor(mOutput.pImpl->mType, mOutput.pImpl->mAux)
        );
    }
}

void ICustomBrewingRecipe::_init() {
    pImpl->mBrewingRecipe.emplace(CustomBrewingRecipeBase(getInput(), getReagent(), getOutput()));
}

} // namespace modapi::inline recipe
