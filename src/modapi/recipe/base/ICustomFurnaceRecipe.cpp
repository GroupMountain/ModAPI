#include "modapi/recipe/base/ICustomFurnaceRecipe.h"
#include "modapi/recipe/SharedTypes.h"

bool operator<(Recipes::FurnaceRecipeKey const& lhs, Recipes::FurnaceRecipeKey const& rhs) { return lhs.mID < rhs.mID; }
bool operator==(Recipes::FurnaceRecipeKey const& lhs, Recipes::FurnaceRecipeKey const& rhs) {
    return lhs.mID == rhs.mID;
}

namespace modapi::inline recipe {

ICustomFurnaceRecipe::ICustomFurnaceRecipe() = default;

std::string ICustomFurnaceRecipe::getRecipeId() const { return {}; }

std::vector<::std::string> ICustomFurnaceRecipe::getCraftingTags() const { return {"furnace"}; }

CustomFurnaceRecipeBase::CustomFurnaceRecipeBase(
    ICustomRecipe::Ingredient const& input,
    ::ItemInstance const&            output,
    std::vector<std::string> const&  craftingTags
)
: mInput(ItemInstance(input.pImpl->mType, input.pImpl->mCount, input.pImpl->mAux)),
  mOutput(output) {
    for (auto& tag : craftingTags) {
        mTags.emplace_back(tag);
    }
}

void CustomFurnaceRecipeBase::registerRecipe(::Recipes& registry) {
    for (auto& tag : mTags) {
        for (auto& [key, val] : *registry.mFurnaceRecipes) {
            if (tag.getString() == key.mTag->getString() && mInput.getIdAux() == key.mID) {
                registry.mFurnaceRecipes->erase(key);
            }
        }
    }
    registry.addFurnaceRecipeAuxData(mInput, mOutput, mTags);
}

void ICustomFurnaceRecipe::_init() {
    pImpl->mFurnaceRecipe.emplace(CustomFurnaceRecipeBase(getIngredient(), getResult(), getCraftingTags()));
}

} // namespace modapi::inline recipe