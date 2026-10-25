#pragma once
#include "modapi/recipe/base/ICustomShapelessMultiRecipe.h"

namespace modapi::inline recipe {

class CustomShapelessMultiRecipe : public ICustomShapelessMultiRecipe {
    std::string              mRecipeId;
    std::vector<Ingredient>  mIngredients;
    ::ItemInstance           mResult;
    CraftingCallback         mCraftingCallback;
    UnlockingRequirement     mUnlock;
    std::vector<std::string> mTags;
    int                      mPriority;

public:
    CustomShapelessMultiRecipe(
        std::string const&                            recipeId,
        std::vector<ICustomRecipe::Ingredient> const& ingredients,
        ::ItemInstance const&                         result,
        CraftingCallback const&                       craftingCallback,
        ICustomRecipe::UnlockingRequirement const&    unlock,
        std::vector<std::string> const&               tags,
        int                                           priority
    )
    : ICustomShapelessMultiRecipe(),
      mRecipeId(recipeId),
      mIngredients(ingredients),
      mResult(result),
      mCraftingCallback(craftingCallback),
      mUnlock(unlock),
      mTags(tags),
      mPriority(priority) {}

    std::string getRecipeId() const override { return mRecipeId; }

    std::vector<std::string> getCraftingTags() const override { return mTags; }

    std::vector<Ingredient> getIngredients() const override { return mIngredients; }

    ::ItemInstance getResult() const override { return mResult; }

    int getPriority() const override { return mPriority; }

    UnlockingRequirement getUnlockingRequirement() const override { return mUnlock; }

    CraftingCallback getCraftingCallback() const override { return mCraftingCallback; }
};

} // namespace modapi::inline recipe
