#include "modapi/recipe/base/ICustomRecipe.h"
#include "modapi/recipe/SharedTypes.h"
#include <mc/world/item/crafting/Recipe.h>

namespace modapi::inline recipe {

ICustomRecipe::Ingredient::Ingredient(const char* type)
: pImpl(std::make_unique<Impl>(type, RecipeIngredientType::Item, 1)) {}

ICustomRecipe::Ingredient::Ingredient(std::string_view type, RecipeIngredientType ingredientType, uint8_t count)
: pImpl(std::make_unique<Impl>(type, ingredientType, count)) {}

ICustomRecipe::Ingredient::Ingredient(std::string_view type, uint8_t count, std::optional<short> aux)
: pImpl(std::make_unique<Impl>(type, count, aux)) {}

ICustomRecipe::Ingredient::~Ingredient() = default;

ICustomRecipe::Ingredient::Ingredient(Ingredient const& other) { pImpl = std::make_unique<Impl>(*other.pImpl); }

ICustomRecipe::Ingredient::Ingredient(Ingredient&&) = default;

ICustomRecipe::Ingredient& ICustomRecipe::Ingredient::operator=(Ingredient const& other) {
    pImpl = std::make_unique<Impl>(*other.pImpl);
    return *this;
}

ICustomRecipe::Ingredient& ICustomRecipe::Ingredient::operator=(Ingredient&&) = default;

ICustomRecipe::UnlockingRequirement::UnlockingRequirement(
    std::vector<Ingredient> const& items,
    RecipeUnlockingContext         context
)
: pImpl(std::make_unique<Impl>(items, context)) {}

ICustomRecipe::UnlockingRequirement::UnlockingRequirement(RecipeUnlockingContext context)
: pImpl(std::make_unique<Impl>(context)) {}

ICustomRecipe::UnlockingRequirement::~UnlockingRequirement() = default;

ICustomRecipe::UnlockingRequirement::UnlockingRequirement(UnlockingRequirement const& other) {
    pImpl = std::make_unique<Impl>(*other.pImpl);
}

ICustomRecipe::UnlockingRequirement::UnlockingRequirement(UnlockingRequirement&&) = default;

ICustomRecipe::UnlockingRequirement& ICustomRecipe::UnlockingRequirement::operator=(UnlockingRequirement const& other) {
    pImpl = std::make_unique<Impl>(*other.pImpl);
    return *this;
}

ICustomRecipe::UnlockingRequirement& ICustomRecipe::UnlockingRequirement::operator=(UnlockingRequirement&&) = default;

ICustomRecipe::ICustomRecipe() : pImpl(std::make_unique<Impl>()) {}

ICustomRecipe::~ICustomRecipe() = default;

} // namespace modapi::inline recipe