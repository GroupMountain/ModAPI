#include "modapi/recipe/base/ICustomBrewingRecipe.h"
#include "modapi/recipe/SharedTypes.h"
#include <mc/world/item/Item.h>
#include <mc/world/item/ItemDescriptor.h>
#include <mc/world/item/ItemStackBase.h>

namespace {

PotionBrewing::Ingredient makePotionIngredient(std::string_view itemName, short aux) {
    PotionBrewing::Ingredient result{};
    result.mItemId = ::ItemDescriptor(itemName, 0).getItem()->mId;
    result.mData   = aux;
    return result;
}

} // namespace


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
            *::ItemDescriptor(mInput.pImpl->mType, 0).getItem(),
            makePotionIngredient(mReagent.pImpl->mType, mReagent.pImpl->mAux),
            *::ItemDescriptor(mOutput.pImpl->mType, 0).getItem()
        );
    } else {
        if (mReagent.pImpl->mAux == 32767) {
            mReagent.pImpl->mAux = 0;
        }
        for (auto& recipe : PotionBrewing::mPotionMixes()) {
            if (recipe.mFrom.sameItem(mInput.pImpl->serialize(), true)
                && recipe.mIngredient.mItemId == makePotionIngredient(mReagent.pImpl->mType, 0).mItemId
                && recipe.mIngredient.mData == mReagent.pImpl->mAux) {
                recipe.mTo = ::ItemDescriptor(mOutput.pImpl->mType, mOutput.pImpl->mAux);
                return;
            }
        }
        PotionBrewing::addPotionMix(
            ::ItemDescriptor(mInput.pImpl->mType, mInput.pImpl->mAux),
            makePotionIngredient(mReagent.pImpl->mType, mReagent.pImpl->mAux),
            ::ItemDescriptor(mOutput.pImpl->mType, mOutput.pImpl->mAux)
        );
    }
}

void ICustomBrewingRecipe::_init() {
    pImpl->mBrewingRecipe.emplace(CustomBrewingRecipeBase(getInput(), getReagent(), getOutput()));
}

} // namespace modapi::inline recipe
