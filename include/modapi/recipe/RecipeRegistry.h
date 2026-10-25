#pragma once
#include "modapi/Macros.h"
#include "modapi/Registration.h"
#include "modapi/Registry.h"
#include "modapi/recipe/base/ICustomRecipe.h"
#include "modapi/recipe/types/CustomBrewingRecipe.h"
#include "modapi/recipe/types/CustomFurnaceRecipe.h"
#include "modapi/recipe/types/CustomJsonRecipe.h"
#include "modapi/recipe/types/CustomShapedMultiRecipe.h"
#include "modapi/recipe/types/CustomShapedRecipe.h"
#include "modapi/recipe/types/CustomShapelessMultiRecipe.h"
#include "modapi/recipe/types/CustomShapelessRecipe.h"
#include "modapi/recipe/types/CustomSmithingTransformRecipe.h"
#include "modapi/recipe/types/CustomSmithingTrimRecipe.h"
#include "modapi/recipe/types/CustomStoneCutterRecipe.h"
#include <concepts>
#include <filesystem>
#include <functional>
#include <ll/api/event/Event.h>
#include <mc/deps/core/utility/optional_ref.h>
#include <mc/world/item/crafting/Recipe.h>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

class Recipe;
class Recipes;

namespace modapi::inline recipe {

class RecipeRegistry;

class RecipeReadyEvent final : public ll::event::Event {
    RecipeRegistry& mRegistry;

public:
    constexpr explicit RecipeReadyEvent(RecipeRegistry& registry) : mRegistry(registry) {}

    MOD_NDAPI RecipeRegistry& registry() const;
};

template <class T>
struct IsConcreteRecipe : std::false_type {};
template <>
struct IsConcreteRecipe<CustomShapedRecipe> : std::true_type {};
template <>
struct IsConcreteRecipe<CustomShapelessRecipe> : std::true_type {};
template <>
struct IsConcreteRecipe<CustomShapedMultiRecipe> : std::true_type {};
template <>
struct IsConcreteRecipe<CustomShapelessMultiRecipe> : std::true_type {};
template <>
struct IsConcreteRecipe<CustomStoneCutterRecipe> : std::true_type {};
template <>
struct IsConcreteRecipe<CustomFurnaceRecipe> : std::true_type {};
template <>
struct IsConcreteRecipe<CustomBrewingRecipe> : std::true_type {};
template <>
struct IsConcreteRecipe<CustomSmithingTransformRecipe> : std::true_type {};
template <>
struct IsConcreteRecipe<CustomSmithingTrimRecipe> : std::true_type {};
template <>
struct IsConcreteRecipe<CustomJsonRecipe> : std::true_type {};

template <class Entry>
class RecipeBuilder;

template <>
class RecipeBuilder<CustomShapedRecipe> {
    std::string                            mIdentifier;
    std::vector<std::string>               mShape;
    ICustomShapedRecipe::ShapedIngredients mIngredients;
    ::ItemInstance                         mResult;
    ICustomRecipe::UnlockingRequirement    mUnlock{RecipeUnlockingContext::AlwaysUnlocked};
    std::vector<std::string>               mTags{"crafting_table"};
    bool                                   mAssumeSymmetry = false;
    int                                    mPriority       = 50;

public:
    explicit RecipeBuilder(std::string identifier) : mIdentifier(std::move(identifier)) {}

    RecipeBuilder& shape(std::vector<std::string> value) {
        mShape = std::move(value);
        return *this;
    }

    RecipeBuilder& ingredients(ICustomShapedRecipe::ShapedIngredients value) {
        mIngredients = std::move(value);
        return *this;
    }

    RecipeBuilder& result(::ItemInstance value) {
        mResult = std::move(value);
        return *this;
    }

    RecipeBuilder& unlock(ICustomRecipe::UnlockingRequirement value) {
        mUnlock = std::move(value);
        return *this;
    }

    RecipeBuilder& tags(std::vector<std::string> value) {
        mTags = std::move(value);
        return *this;
    }

    RecipeBuilder& assumeSymmetry(bool value) {
        mAssumeSymmetry = value;
        return *this;
    }

    RecipeBuilder& priority(int value) {
        mPriority = value;
        return *this;
    }

    [[nodiscard]] ClassRegistration<
        CustomShapedRecipe,
        decltype(mShape),
        decltype(mIngredients),
        decltype(mResult),
        decltype(mUnlock),
        decltype(mTags),
        decltype(mAssumeSymmetry),
        decltype(mPriority)>
    build() const {
        return {
            mIdentifier,
            {mShape, mIngredients, mResult, mUnlock, mTags, mAssumeSymmetry, mPriority}
        };
    }
};

template <>
class RecipeBuilder<CustomShapelessRecipe> {
    std::string                            mIdentifier;
    std::vector<ICustomRecipe::Ingredient> mIngredients;
    ::ItemInstance                         mResult;
    ICustomRecipe::UnlockingRequirement    mUnlock{RecipeUnlockingContext::AlwaysUnlocked};
    std::vector<std::string>               mTags{"crafting_table"};
    int                                    mPriority = 50;

public:
    explicit RecipeBuilder(std::string identifier) : mIdentifier(std::move(identifier)) {}

    RecipeBuilder& ingredients(std::vector<ICustomRecipe::Ingredient> value) {
        mIngredients = std::move(value);
        return *this;
    }

    RecipeBuilder& result(::ItemInstance value) {
        mResult = std::move(value);
        return *this;
    }

    RecipeBuilder& unlock(ICustomRecipe::UnlockingRequirement value) {
        mUnlock = std::move(value);
        return *this;
    }

    RecipeBuilder& tags(std::vector<std::string> value) {
        mTags = std::move(value);
        return *this;
    }

    RecipeBuilder& priority(int value) {
        mPriority = value;
        return *this;
    }

    [[nodiscard]] auto build() const {
        return ClassRegistration<
            CustomShapelessRecipe,
            decltype(mIngredients),
            decltype(mResult),
            decltype(mUnlock),
            decltype(mTags),
            decltype(mPriority)>{
            mIdentifier,
            {mIngredients, mResult, mUnlock, mTags, mPriority}
        };
    }
};

template <>
class RecipeBuilder<CustomShapedMultiRecipe> {
    std::string                                mIdentifier;
    std::vector<std::string>                   mShape;
    ICustomShapedRecipe::ShapedIngredients     mIngredients;
    ::ItemInstance                             mResult;
    ICustomShapedMultiRecipe::CraftingCallback mCallback;
    ICustomRecipe::UnlockingRequirement        mUnlock{RecipeUnlockingContext::AlwaysUnlocked};
    std::vector<std::string>                   mTags{"crafting_table"};
    bool                                       mAssumeSymmetry = false;
    int                                        mPriority       = 50;

public:
    explicit RecipeBuilder(std::string identifier) : mIdentifier(std::move(identifier)) {}

    RecipeBuilder& shape(std::vector<std::string> value) {
        mShape = std::move(value);
        return *this;
    }

    RecipeBuilder& ingredients(ICustomShapedRecipe::ShapedIngredients value) {
        mIngredients = std::move(value);
        return *this;
    }

    RecipeBuilder& result(::ItemInstance value) {
        mResult = std::move(value);
        return *this;
    }

    RecipeBuilder& callback(ICustomShapedMultiRecipe::CraftingCallback value) {
        mCallback = std::move(value);
        return *this;
    }

    RecipeBuilder& unlock(ICustomRecipe::UnlockingRequirement value) {
        mUnlock = std::move(value);
        return *this;
    }

    RecipeBuilder& tags(std::vector<std::string> value) {
        mTags = std::move(value);
        return *this;
    }

    RecipeBuilder& assumeSymmetry(bool value) {
        mAssumeSymmetry = value;
        return *this;
    }

    RecipeBuilder& priority(int value) {
        mPriority = value;
        return *this;
    }

    [[nodiscard]] auto build() const {
        return ClassRegistration<
            CustomShapedMultiRecipe,
            decltype(mShape),
            decltype(mIngredients),
            decltype(mResult),
            decltype(mCallback),
            decltype(mUnlock),
            decltype(mTags),
            decltype(mAssumeSymmetry),
            decltype(mPriority)>{
            mIdentifier,
            {mShape, mIngredients, mResult, mCallback, mUnlock, mTags, mAssumeSymmetry, mPriority}
        };
    }
};

template <>
class RecipeBuilder<CustomShapelessMultiRecipe> {
    std::string                                   mIdentifier;
    std::vector<ICustomRecipe::Ingredient>        mIngredients;
    ::ItemInstance                                mResult;
    ICustomShapelessMultiRecipe::CraftingCallback mCallback;
    ICustomRecipe::UnlockingRequirement           mUnlock{RecipeUnlockingContext::AlwaysUnlocked};
    std::vector<std::string>                      mTags{"crafting_table"};
    int                                           mPriority = 50;

public:
    explicit RecipeBuilder(std::string identifier) : mIdentifier(std::move(identifier)) {}

    RecipeBuilder& ingredients(std::vector<ICustomRecipe::Ingredient> value) {
        mIngredients = std::move(value);
        return *this;
    }

    RecipeBuilder& result(::ItemInstance value) {
        mResult = std::move(value);
        return *this;
    }

    RecipeBuilder& callback(ICustomShapelessMultiRecipe::CraftingCallback value) {
        mCallback = std::move(value);
        return *this;
    }

    RecipeBuilder& unlock(ICustomRecipe::UnlockingRequirement value) {
        mUnlock = std::move(value);
        return *this;
    }

    RecipeBuilder& tags(std::vector<std::string> value) {
        mTags = std::move(value);
        return *this;
    }

    RecipeBuilder& priority(int value) {
        mPriority = value;
        return *this;
    }

    [[nodiscard]] auto build() const {
        return ClassRegistration<
            CustomShapelessMultiRecipe,
            decltype(mIngredients),
            decltype(mResult),
            decltype(mCallback),
            decltype(mUnlock),
            decltype(mTags),
            decltype(mPriority)>{
            mIdentifier,
            {mIngredients, mResult, mCallback, mUnlock, mTags, mPriority}
        };
    }
};

template <>
class RecipeBuilder<CustomStoneCutterRecipe> {
    std::string               mIdentifier;
    ICustomRecipe::Ingredient mIngredient{""};
    ::ItemInstance            mResult;
    int                       mPriority = 50;

public:
    explicit RecipeBuilder(std::string identifier) : mIdentifier(std::move(identifier)) {}

    RecipeBuilder& ingredient(ICustomRecipe::Ingredient value) {
        mIngredient = std::move(value);
        return *this;
    }

    RecipeBuilder& result(::ItemInstance value) {
        mResult = std::move(value);
        return *this;
    }

    RecipeBuilder& priority(int value) {
        mPriority = value;
        return *this;
    }

    [[nodiscard]] auto build() const {
        return ClassRegistration<CustomStoneCutterRecipe, decltype(mIngredient), decltype(mResult), int>{
            mIdentifier,
            {mIngredient, mResult, mPriority}
        };
    }
};

template <>
class RecipeBuilder<CustomFurnaceRecipe> {
    std::string               mIdentifier;
    ICustomRecipe::Ingredient mInput{""};
    ::ItemInstance            mOutput;
    std::vector<std::string>  mTags{"furnace"};

public:
    explicit RecipeBuilder(std::string identifier) : mIdentifier(std::move(identifier)) {}

    RecipeBuilder& ingredient(ICustomRecipe::Ingredient value) {
        mInput = std::move(value);
        return *this;
    }

    RecipeBuilder& result(::ItemInstance value) {
        mOutput = std::move(value);
        return *this;
    }

    RecipeBuilder& tags(std::vector<std::string> value) {
        mTags = std::move(value);
        return *this;
    }

    [[nodiscard]] auto build() const {
        return ClassRegistration<CustomFurnaceRecipe, decltype(mInput), decltype(mOutput), decltype(mTags)>{
            mIdentifier,
            {mInput, mOutput, mTags}
        };
    }
};

template <>
class RecipeBuilder<CustomBrewingRecipe> {
    std::string               mIdentifier;
    ICustomRecipe::Ingredient mInput{""};
    ICustomRecipe::Ingredient mReagent{""};
    ICustomRecipe::Ingredient mOutput{""};

public:
    explicit RecipeBuilder(std::string identifier) : mIdentifier(std::move(identifier)) {}

    RecipeBuilder& input(ICustomRecipe::Ingredient value) {
        mInput = std::move(value);
        return *this;
    }

    RecipeBuilder& reagent(ICustomRecipe::Ingredient value) {
        mReagent = std::move(value);
        return *this;
    }

    RecipeBuilder& output(ICustomRecipe::Ingredient value) {
        mOutput = std::move(value);
        return *this;
    }

    [[nodiscard]] auto build() const {
        return ClassRegistration<CustomBrewingRecipe, decltype(mInput), decltype(mReagent), decltype(mOutput)>{
            mIdentifier,
            {mInput, mReagent, mOutput}
        };
    }
};

template <>
class RecipeBuilder<CustomSmithingTransformRecipe> {
    std::string               mIdentifier;
    ICustomRecipe::Ingredient mSmithingTemplate{""};
    ICustomRecipe::Ingredient mBaseIngredient{""};
    ICustomRecipe::Ingredient mAdditionIngredient{""};
    ::ItemInstance            mResult;

public:
    explicit RecipeBuilder(std::string identifier) : mIdentifier(std::move(identifier)) {}

    RecipeBuilder& smithingTemplate(ICustomRecipe::Ingredient value) {
        mSmithingTemplate = std::move(value);
        return *this;
    }

    RecipeBuilder& baseIngredient(ICustomRecipe::Ingredient value) {
        mBaseIngredient = std::move(value);
        return *this;
    }

    RecipeBuilder& additionIngredient(ICustomRecipe::Ingredient value) {
        mAdditionIngredient = std::move(value);
        return *this;
    }

    RecipeBuilder& result(::ItemInstance value) {
        mResult = std::move(value);
        return *this;
    }

    [[nodiscard]] auto build() const {
        return ClassRegistration<
            CustomSmithingTransformRecipe,
            decltype(mSmithingTemplate),
            decltype(mBaseIngredient),
            decltype(mAdditionIngredient),
            decltype(mResult)>{
            mIdentifier,
            {mSmithingTemplate, mBaseIngredient, mAdditionIngredient, mResult}
        };
    }
};

template <>
class RecipeBuilder<CustomSmithingTrimRecipe> {
    std::string               mIdentifier;
    ICustomRecipe::Ingredient mSmithingTemplate{""};
    ICustomRecipe::Ingredient mBaseIngredient{""};
    ICustomRecipe::Ingredient mAdditionIngredient{""};

public:
    explicit RecipeBuilder(std::string identifier) : mIdentifier(std::move(identifier)) {}

    RecipeBuilder& smithingTemplate(ICustomRecipe::Ingredient value) {
        mSmithingTemplate = std::move(value);
        return *this;
    }

    RecipeBuilder& baseIngredient(ICustomRecipe::Ingredient value) {
        mBaseIngredient = std::move(value);
        return *this;
    }

    RecipeBuilder& additionIngredient(ICustomRecipe::Ingredient value) {
        mAdditionIngredient = std::move(value);
        return *this;
    }

    [[nodiscard]] auto build() const {
        return ClassRegistration<
            CustomSmithingTrimRecipe,
            decltype(mSmithingTemplate),
            decltype(mBaseIngredient),
            decltype(mAdditionIngredient)>{
            mIdentifier,
            {mSmithingTemplate, mBaseIngredient, mAdditionIngredient}
        };
    }
};

template <>
class RecipeBuilder<CustomJsonRecipe> {
    std::string mIdentifier;
    std::string mJson;

public:
    explicit RecipeBuilder(std::string identifier) : mIdentifier(std::move(identifier)) {}

    RecipeBuilder& json(std::string value) {
        mJson = std::move(value);
        return *this;
    }

    [[nodiscard]] auto build() const { return ClassRegistration<CustomJsonRecipe, std::string>{mIdentifier, {mJson}}; }
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

    MOD_NDAPI bool isReady() const noexcept;

    MOD_API static void ensureEventRegistered();

    template <class Entry>
    static constexpr bool isEntry = std::derived_from<Entry, ICustomRecipe>;

    template <class Entry>
    static auto builder(std::string identifier) {
        if constexpr (IsConcreteRecipe<Entry>::value) {
            return RecipeBuilder<Entry>{std::move(identifier)};
        } else {
            return ClassRegistrationBuilder<RecipeRegistry, Entry>{std::move(identifier)};
        }
    }

    template <class Entry, class... Args>
        requires isEntry<Entry>
    ProductRef registerEntry(ClassRegistration<Entry, Args...> registration) {
        if constexpr (std::same_as<Entry, CustomJsonRecipe>) {
            return registerRecipeFromMemoryJson(std::get<0>(registration.mArguments));
        } else if constexpr (IsConcreteRecipe<Entry>::value) {
            return _registerRecipe(
                [identifier = registration.mIdentifier,
                 arguments  = registration.mArguments]() mutable -> std::unique_ptr<ICustomRecipe> {
                    return std::apply(
                        [&identifier](auto&&... args) -> std::unique_ptr<ICustomRecipe> {
                            return std::make_unique<Entry>(identifier, std::forward<decltype(args)>(args)...);
                        },
                        arguments
                    );
                }
            );
        } else {
            return _registerRecipe([arguments = registration.mArguments]() mutable -> std::unique_ptr<ICustomRecipe> {
                return std::apply(
                    [](auto&&... args) -> std::unique_ptr<ICustomRecipe> {
                        return std::make_unique<Entry>(std::forward<decltype(args)>(args)...);
                    },
                    arguments
                );
            });
        }
    }

    MOD_API bool unregisterRecipe(std::string const& recipeId, bool updateClients = false);

    MOD_API void updateClientRecipes();

    MOD_NDAPI ProductRef registerRecipeFromMemoryJson(std::string const& rawJson);

    MOD_NDAPI ProductRef registerRecipeFromJsonFile(std::filesystem::path const& jsonPath);

    MOD_NDAPI ProductRef _registerRecipe(std::function<std::unique_ptr<ICustomRecipe>()>&&);

    MOD_API void _bindRegistry(::Recipes& recipes);
};

} // namespace modapi::inline recipe

static_assert(modapi::Registry<modapi::RecipeRegistry>);
