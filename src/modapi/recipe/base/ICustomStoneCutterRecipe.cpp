#include "modapi/recipe/base/ICustomStoneCutterRecipe.h"
#include "modapi/recipe/SharedTypes.h"

namespace modapi::inline recipe {

ICustomStoneCutterRecipe::ICustomStoneCutterRecipe() : ICustomShapelessRecipe() {}

std::vector<std::string> ICustomStoneCutterRecipe::getCraftingTags() const { return {"stonecutter"}; }

std::vector<ICustomRecipe::Ingredient> ICustomStoneCutterRecipe::getIngredients() const { return {getIngredient()}; }

} // namespace modapi::inline recipe
