#pragma once
#include "modapi/recipe/base/ICustomSmithingTransformRecipe.h"

namespace modapi::inline recipe {

class CustomSmithingTransformRecipe : public ICustomSmithingTransformRecipe {
    std::string    mRecipeId;
    Ingredient     mSmithingTemplate;
    Ingredient     mBaseIngredient;
    Ingredient     mAdditionIngredient;
    ::ItemInstance mResult;

public:
    CustomSmithingTransformRecipe(
        std::string const&               recipeId,
        ICustomRecipe::Ingredient const& smithingTemplate,
        ICustomRecipe::Ingredient const& baseIngredient,
        ICustomRecipe::Ingredient const& additionIngredient,
        ::ItemInstance const&            result
    )
    : ICustomSmithingTransformRecipe(),
      mRecipeId(recipeId),
      mSmithingTemplate(smithingTemplate),
      mBaseIngredient(baseIngredient),
      mAdditionIngredient(additionIngredient),
      mResult(result) {}

    std::string getRecipeId() const override { return mRecipeId; }

    Ingredient getSmithingTemplate() const override { return mSmithingTemplate; }

    Ingredient getBaseIngredient() const override { return mBaseIngredient; }

    Ingredient getAdditionIngredient() const override { return mAdditionIngredient; }

    ::ItemInstance getResult() const override { return mResult; }
};

} // namespace modapi::inline recipe
