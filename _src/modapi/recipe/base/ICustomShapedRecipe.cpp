#include "gmlib/mod/recipe/SharedTypes.h"
#include <gmlib/mod/recipe/base/ICustomShapedRecipe.h>
#include <mc/world/item/crafting/Recipe.h>

namespace gmlib::mod {

ICustomShapedRecipe::ShapedIngredients::ShapedIngredients() {}

ICustomShapedRecipe::ShapedIngredients::ShapedIngredients(
    std::unordered_map<std::string, Ingredient> const& ingredients
)
: mIngredients(ingredients) {}

ICustomShapedRecipe::ShapedIngredients::ShapedIngredients(
    std::initializer_list<std::pair<const std::string, Ingredient>> init
)
: mIngredients(init) {}

void ICustomShapedRecipe::ShapedIngredients::add(std::string_view key, Ingredient const& ingredient) {
    mIngredients.emplace(key, ingredient);
}

CustomShapedRecipeBase::CustomShapedRecipeBase(
    std::string_view                             craftingTag,
    std::string_view                             recipeId,
    std::vector<std::string> const&              shape,
    ICustomShapedRecipe::ShapedIngredients const ingredients,
    ::ItemInstance const&                        result,
    int                                          priority,
    ICustomRecipe::UnlockingRequirement const    unlock,
    bool                                         assumeSymmetry
)
: ShapedRecipe() {
    mTag      = HashedString(craftingTag);
    mRecipeId = recipeId;
    mWidth    = 0;
    mHeight   = (int)shape.size();
    for (auto shapeline : shape) {
        if (shapeline.size() > mWidth) {
            mWidth = (int)shapeline.size();
        }
        for (auto& c : shapeline) {
            auto key = std::string(1, c);
            if (ingredients.mIngredients.contains(key)) {
                mMyIngredients->push_back(ingredients.mIngredients.at(key).pImpl->serialize());
            } else {
                mMyIngredients->push_back(RecipeIngredient::EMPTY_INGREDIENT());
            }
        }
    }
    mResults->mResultsAreLoaded = true;
    mResults->mResults->push_back(result);
    mPriority             = priority;
    mUnlockingRequirement = unlock.pImpl->serialize();
    mAssumeSymmetry       = assumeSymmetry;
}

ICustomShapedRecipe::ICustomShapedRecipe() = default;

void ICustomShapedRecipe::_init() {
    auto tags = getCraftingTags();
    pImpl->mRecipes.reserve(tags.size());
    for (auto const& tag : tags) {
        auto recipe = std::make_unique<CustomShapedRecipeBase>(
            tag,
            getRecipeId(),
            getShape(),
            getIngredients(),
            getResult(),
            getPriority(),
            getUnlockingRequirement(),
            isAssumeSymmetry()
        );
        pImpl->mRecipes.push_back(std::move(recipe));
    }
}

std::vector<::std::string> ICustomShapedRecipe::getCraftingTags() const { return {"crafting_table"}; }

int ICustomShapedRecipe::getPriority() const { return 50; }

ICustomRecipe::UnlockingRequirement ICustomShapedRecipe::getUnlockingRequirement() const {
    return {RecipeUnlockingContext::AlwaysUnlocked};
}

bool ICustomShapedRecipe::isAssumeSymmetry() const { return true; }

} // namespace gmlib::mod