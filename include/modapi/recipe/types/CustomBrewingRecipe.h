#pragma once
#include "modapi/recipe/base/ICustomBrewingRecipe.h"

namespace modapi::inline recipe {

class CustomBrewingRecipe : public ICustomBrewingRecipe {
    std::string mRecipeId;
    Ingredient  mInput;
    Ingredient  mReagent;
    Ingredient  mOutput;

public:
    CustomBrewingRecipe(
        std::string const& recipeId,
        Ingredient const&  input,
        Ingredient const&  reagent,
        Ingredient const&  output
    )
    : ICustomBrewingRecipe(),
      mRecipeId(recipeId),
      mInput(input),
      mReagent(reagent),
      mOutput(output) {}

    std::string getRecipeId() const override { return mRecipeId; }

    Ingredient getInput() const override { return mInput; }

    Ingredient getReagent() const override { return mReagent; }

    Ingredient getOutput() const override { return mOutput; }
};

} // namespace modapi::inline recipe
