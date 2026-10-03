#pragma once
#include "modapi/Macros.h"
#include "modapi/Registry.h"
#include "modapi/recipe/base/ICustomShapedMultiRecipe.h"
#include "modapi/recipe/base/ICustomShapelessMultiRecipe.h"
#include <concepts>
#include <filesystem>
#include <functional>
#include <ll/api/event/Event.h>
#include <mc/deps/core/utility/optional_ref.h>
#include <mc/world/item/crafting/Recipe.h>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

class Recipe;
class Recipes;

namespace modapi::inline recipe {

class RecipeRegistry;

// Published at the end of `Recipes::init`, which `ServerLevel::initialize` calls for every level it
// loads: recipes live in the `Recipes` object of that level, so the deferred registrations are
// replayed for the instance the event carries.
class RecipeReadyEvent final : public ll::event::Event {
    RecipeRegistry& mRegistry;

public:
    constexpr explicit RecipeReadyEvent(RecipeRegistry& registry) : mRegistry(registry) {}

    MOD_NDAPI RecipeRegistry& registry() const;
};

class RecipeRegistry {
public:
    struct Impl;
    std::unique_ptr<Impl> pImpl;

public:
    using EventType  = RecipeReadyEvent;
    using Product    = ::Recipe;
    using ProductRef = optional_ref<::Recipe>;

public:
    RecipeRegistry();
    RecipeRegistry& operator=(RecipeRegistry const&) = delete;
    RecipeRegistry(RecipeRegistry const&)            = delete;

public:
    MOD_NDAPI static RecipeRegistry& getInstance();

    // True once a `Recipes` instance has been initialised, i.e. once recipes can be added.
    MOD_NDAPI bool isReady() const noexcept;

    // Registers the ready event emitter (idempotent); called by `ModAPI::load()` and by every
    // `DeferredRegister<RecipeRegistry, ...>`.
    MOD_API static void ensureEventRegistered();

    // Entry types are ICustomRecipe implementations.
    template <class Entry>
    static constexpr bool isEntry = std::derived_from<Entry, ICustomRecipe>;

    // Builds `Entry` and registers it into the current Recipes instance.
    template <std::derived_from<ICustomRecipe> Entry, class... Args>
    ProductRef registerEntry(Args&&... args) {
        return registerRecipe<Entry>(std::forward<Args>(args)...);
    }

    template <std::derived_from<ICustomRecipe> Entry, class... Args>
    ProductRef registerRecipe(Args&&... args) {
        return _registerRecipe([... args = std::forward<Args>(args)]() -> std::unique_ptr<ICustomRecipe> {
            return std::make_unique<Entry>(args...);
        });
    }

    MOD_NDAPI ProductRef registerShapedRecipe(
        std::string const&                            recipeId,
        std::vector<std::string> const&               shape,
        ICustomShapedRecipe::ShapedIngredients const& ingredients,
        ::ItemInstance const&                         result,
        ICustomRecipe::UnlockingRequirement const&    unlock =
            ICustomRecipe::UnlockingRequirement(RecipeUnlockingContext::AlwaysUnlocked),
        std::vector<std::string> const& tags           = {"crafting_table"},
        bool                            assumeSymmetry = false,
        int                             priority       = 50
    );

    MOD_NDAPI ProductRef registerShapedMultiRecipe(
        std::string const&                            recipeId,
        std::vector<std::string> const&               shape,
        ICustomShapedRecipe::ShapedIngredients const& ingredients,
        ::ItemInstance const&                         result,
        ICustomShapedMultiRecipe::CraftingCallback&&  craftingCallback,
        ICustomRecipe::UnlockingRequirement const&    unlock =
            ICustomRecipe::UnlockingRequirement(RecipeUnlockingContext::AlwaysUnlocked),
        std::vector<std::string> const& tags           = {"crafting_table"},
        bool                            assumeSymmetry = false,
        int                             priority       = 50
    );

    MOD_NDAPI ProductRef registerShapelessRecipe(
        std::string const&                            recipeId,
        std::vector<ICustomRecipe::Ingredient> const& ingredients,
        ::ItemInstance const&                         result,
        ICustomRecipe::UnlockingRequirement const&    unlock =
            ICustomRecipe::UnlockingRequirement(RecipeUnlockingContext::AlwaysUnlocked),
        std::vector<std::string> const& tags     = {"crafting_table"},
        int                             priority = 50
    );

    MOD_NDAPI ProductRef registerShapelessMultiRecipe(
        std::string const&                              recipeId,
        std::vector<ICustomRecipe::Ingredient> const&   ingredients,
        ::ItemInstance const&                           result,
        ICustomShapelessMultiRecipe::CraftingCallback&& craftingCallback,
        ICustomRecipe::UnlockingRequirement const&      unlock =
            ICustomRecipe::UnlockingRequirement(RecipeUnlockingContext::AlwaysUnlocked),
        std::vector<std::string> const& tags     = {"crafting_table"},
        int                             priority = 50
    );

    MOD_NDAPI ProductRef registerStoneCutterRecipe(
        std::string const&               recipeId,
        ICustomRecipe::Ingredient const& ingredient,
        ::ItemInstance const&            result,
        int                              priority = 50
    );

    // Furnace and brewing recipes are not `Recipe` instances, so they have no product.
    MOD_NDAPI ProductRef registerFurnaceRecipe(
        ICustomRecipe::Ingredient const& input,
        ::ItemInstance const&            output,
        std::vector<std::string> const&  tags = {"furnace"}
    );

    // Furnace and brewing recipes are not `Recipe` instances, so they have no product.
    MOD_NDAPI ProductRef registerBrewingRecipe(
        ICustomRecipe::Ingredient const& input,
        ICustomRecipe::Ingredient const& reagent,
        ICustomRecipe::Ingredient const& output
    );

    MOD_NDAPI ProductRef registerSmithingTransformRecipe(
        std::string const&               recipeId,
        ICustomRecipe::Ingredient const& smithingTemplate,
        ICustomRecipe::Ingredient const& baseIngredient,
        ICustomRecipe::Ingredient const& additionIngredient,
        ::ItemInstance const&            result
    );

    MOD_NDAPI ProductRef registerSmithingTrimRecipe(
        std::string const&               recipeId,
        ICustomRecipe::Ingredient const& smithingTemplate,
        ICustomRecipe::Ingredient const& baseIngredient,
        ICustomRecipe::Ingredient const& additionIngredient
    );

    MOD_API bool unregisterRecipe(std::string const& recipeId, bool updateClients = false);

    MOD_API void updateClientRecipes();

    MOD_NDAPI ProductRef registerRecipeFromMemoryJson(std::string const& rawJson);

    MOD_NDAPI ProductRef registerRecipeFromJsonFile(std::filesystem::path const& jsonPath);

    // Builds the custom recipe and registers it into the current Recipes instance.
    MOD_NDAPI ProductRef _registerRecipe(std::function<std::unique_ptr<ICustomRecipe>()>&&);

    // Binds the instance vanilla is filling and publishes `RecipeReadyEvent`.
    MOD_API void _bindRegistry(::Recipes& recipes);
};

} // namespace modapi::inline recipe

static_assert(modapi::Registry<modapi::RecipeRegistry>);
