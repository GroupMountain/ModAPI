#pragma once
#include "modapi/recipe/base/ICustomShapelessRecipe.h"

namespace modapi::inline recipe {

class CustomShapelessRecipe : public ICustomShapelessRecipe {
    std::string              mRecipeId;
    std::vector<Ingredient>  mIngredients;
    ::ItemInstance           mResult;
    UnlockingRequirement     mUnlock;
    std::vector<std::string> mTags;
    int                      mPriority;

public:
    CustomShapelessRecipe(
        std::string const&                         recipeId,
        std::vector<Ingredient> const&             ingredients,
        ::ItemInstance const&                      result,
        ICustomRecipe::UnlockingRequirement const& unlock,
        std::vector<std::string> const&            tags,
        int                                        priority
    )
    : ICustomShapelessRecipe(),
      mRecipeId(recipeId),
      mIngredients(ingredients),
      mResult(result),
      mUnlock(unlock),
      mTags(tags),
      mPriority(priority) {}

    std::string getRecipeId() const override { return mRecipeId; }

    std::vector<std::string> getCraftingTags() const override { return mTags; }

    std::vector<Ingredient> getIngredients() const override { return mIngredients; }

    ::ItemInstance getResult() const override { return mResult; }

    int getPriority() const override { return mPriority; }

    UnlockingRequirement getUnlockingRequirement() const override { return mUnlock; }
};

} // namespace modapi::inline recipe
