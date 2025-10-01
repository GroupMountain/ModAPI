#include "gmlib/mod/recipe/SharedTypes.h"
#include <gmlib/mod/recipe/base/ICustomStoneCutterRecipe.h>

namespace gmlib::mod {

ICustomStoneCutterRecipe::ICustomStoneCutterRecipe() : ICustomShapelessRecipe() {}

std::vector<std::string> ICustomStoneCutterRecipe::getCraftingTags() const { return {"stonecutter"}; }

std::vector<ICustomRecipe::Ingredient> ICustomStoneCutterRecipe::getIngredients() const { return {getIngredient()}; }

} // namespace gmlib::mod
