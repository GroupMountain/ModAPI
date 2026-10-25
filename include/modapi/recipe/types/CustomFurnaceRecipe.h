#pragma once
#include "modapi/recipe/base/ICustomFurnaceRecipe.h"

namespace modapi::inline recipe {

class CustomFurnaceRecipe : public ICustomFurnaceRecipe {
    std::string              mRecipeId;
    Ingredient               mInput;
    ::ItemInstance           mOutput;
    std::vector<std::string> mTags;

public:
    CustomFurnaceRecipe(
        std::string const&              recipeId,
        Ingredient const&               input,
        ::ItemInstance const&           output,
        std::vector<std::string> const& tags
    )
    : ICustomFurnaceRecipe(),
      mRecipeId(recipeId),
      mInput(input),
      mOutput(output),
      mTags(tags) {}

    std::string getRecipeId() const override { return mRecipeId; }

    std::vector<std::string> getCraftingTags() const override { return mTags; }

    Ingredient getIngredient() const override { return mInput; }

    ::ItemInstance getResult() const override { return mOutput; }
};

} // namespace modapi::inline recipe
