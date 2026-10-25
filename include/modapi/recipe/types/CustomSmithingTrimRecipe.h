#pragma once
#include "modapi/recipe/base/ICustomSmithingTrimRecipe.h"

namespace modapi::inline recipe {

class CustomSmithingTrimRecipe : public ICustomSmithingTrimRecipe {
    std::string mRecipeId;
    Ingredient  mSmithingTemplate;
    Ingredient  mBaseIngredient;
    Ingredient  mAdditionIngredient;

public:
    CustomSmithingTrimRecipe(
        std::string const&               recipeId,
        ICustomRecipe::Ingredient const& smithingTemplate,
        ICustomRecipe::Ingredient const& baseIngredient,
        ICustomRecipe::Ingredient const& additionIngredient
    )
    : ICustomSmithingTrimRecipe(),
      mRecipeId(recipeId),
      mSmithingTemplate(smithingTemplate),
      mBaseIngredient(baseIngredient),
      mAdditionIngredient(additionIngredient) {}

    std::string getRecipeId() const override { return mRecipeId; }

    Ingredient getSmithingTemplate() const override { return mSmithingTemplate; }

    Ingredient getBaseIngredient() const override { return mBaseIngredient; }

    Ingredient getAdditionIngredient() const override { return mAdditionIngredient; }
};

} // namespace modapi::inline recipe
