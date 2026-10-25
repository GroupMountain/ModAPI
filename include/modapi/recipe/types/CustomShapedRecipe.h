#pragma once
#include "modapi/recipe/base/ICustomShapedRecipe.h"

namespace modapi::inline recipe {

class CustomShapedRecipe : public ICustomShapedRecipe {
    std::string              mRecipeId;
    std::vector<std::string> mShape;
    ShapedIngredients        mIngredients;
    ::ItemInstance           mResult;
    UnlockingRequirement     mUnlock;
    std::vector<std::string> mTags;
    bool                     mAssumeSymmetry;
    int                      mPriority;

public:
    CustomShapedRecipe(
        std::string const&                            recipeId,
        std::vector<std::string> const&               shape,
        ICustomShapedRecipe::ShapedIngredients const& ingredients,
        ::ItemInstance const&                         result,
        ICustomRecipe::UnlockingRequirement const&    unlock,
        std::vector<std::string> const&               tags,
        bool                                          assumeSymmetry,
        int                                           priority
    )
    : ICustomShapedRecipe(),
      mRecipeId(recipeId),
      mShape(shape),
      mIngredients(ingredients),
      mResult(result),
      mUnlock(unlock),
      mTags(tags),
      mAssumeSymmetry(assumeSymmetry),
      mPriority(priority) {}

    std::string getRecipeId() const override { return mRecipeId; }

    std::vector<std::string> getCraftingTags() const override { return mTags; }

    std::vector<std::string> getShape() const override { return mShape; }

    ShapedIngredients getIngredients() const override { return mIngredients; }

    ::ItemInstance getResult() const override { return mResult; }

    int getPriority() const override { return mPriority; }

    UnlockingRequirement getUnlockingRequirement() const override { return mUnlock; }

    bool isAssumeSymmetry() const override { return mAssumeSymmetry; }
};

} // namespace modapi::inline recipe
