#pragma once
#include "modapi/recipe/base/ICustomStoneCutterRecipe.h"

namespace modapi::inline recipe {

class CustomStoneCutterRecipe : public ICustomStoneCutterRecipe {
    std::string    mRecipeId;
    Ingredient     mIngredient;
    ::ItemInstance mResult;
    int            mPriority;

public:
    CustomStoneCutterRecipe(
        std::string const&    recipeId,
        Ingredient const&     ingredient,
        ::ItemInstance const& result,
        int                   priority
    )
    : ICustomStoneCutterRecipe(),
      mRecipeId(recipeId),
      mIngredient(ingredient),
      mResult(result),
      mPriority(priority) {}

    std::string getRecipeId() const override { return mRecipeId; }

    Ingredient getIngredient() const override { return mIngredient; }

    ::ItemInstance getResult() const override { return mResult; }

    int getPriority() const override { return mPriority; }
};

} // namespace modapi::inline recipe
