#pragma once
#include "modapi/recipe/CustomRecipeRegistry.h"
#include "modapi/recipe/base/ICustomRecipe.h"
#include "modapi/recipe/base/ICustomShapedRecipe.h"
#include "modapi/recipe/shared_types/PotionBrewing.h"
#include "modapi/recipe/shared_types/ShapedRecipe.h"
#include "modapi/recipe/shared_types/ShapelessRecipe.h"
#include "modapi/recipe/shared_types/SmithingTransformRecipe.h"
#include "modapi/recipe/shared_types/SmithingTrimRecipe.h"
#include <mc/common/SharedConstants.h>
#include <mc/deps/core/sem_ver/SemVersion.h>
#include <mc/world/item/ItemInstance.h>
#include <mc/world/item/ItemStack.h>
#include <mc/world/item/ItemTag.h>
#include <mc/world/item/NetworkItemInstanceDescriptor.h>
#include <mc/world/item/crafting/Recipes.h>

namespace modapi::inline recipe {

struct CustomFurnaceRecipeBase {
    ::ItemInstance            mInput;
    ::ItemInstance            mOutput;
    std::vector<HashedString> mTags;

    CustomFurnaceRecipeBase(
        ICustomRecipe::Ingredient const& input,
        ::ItemInstance const&            output,
        std::vector<std::string> const&  craftingTags
    );

    void registerRecipe(::Recipes& registry);
};

struct CustomBrewingRecipeBase {
    ICustomRecipe::Ingredient mInput;
    ICustomRecipe::Ingredient mReagent;
    ICustomRecipe::Ingredient mOutput;

    CustomBrewingRecipeBase(
        ICustomRecipe::Ingredient const& input,
        ICustomRecipe::Ingredient const& reagent,
        ICustomRecipe::Ingredient const& output
    );

    void registerRecipe();
};

struct ICustomRecipe::Impl {
    std::vector<std::unique_ptr<Recipe>>   mRecipes{};
    std::optional<CustomFurnaceRecipeBase> mFurnaceRecipe{};
    std::optional<CustomBrewingRecipeBase> mBrewingRecipe{};
};

struct ICustomRecipe::Ingredient::Impl {
    std::string mType;
    uint8_t     mCount{1};
    short       mAux{32767};
    bool        mIsRecipeTag{false};

    Impl(std::string_view type, RecipeIngredientType ingredientType, uint8_t count)
    : mType(type),
      mCount(count),
      mIsRecipeTag((bool)ingredientType) {}
    Impl(std::string_view type, uint8_t count, std::optional<short> aux) : mType(type), mCount(count) {
        if (aux) mAux = *aux;
    }

    ::RecipeIngredient serialize() const {
        return mIsRecipeTag ? ::RecipeIngredient(::ItemTag(mType), mCount) : ::RecipeIngredient(mType, mAux, mCount);
    }
};

struct ICustomRecipe::UnlockingRequirement::Impl {
    RecipeUnlockingContext  mContext{RecipeUnlockingContext::None};
    std::vector<Ingredient> mItems;

    Impl(std::vector<Ingredient> const& items, RecipeUnlockingContext context) : mContext(context), mItems(items) {}
    Impl(RecipeUnlockingContext context) : mContext(context) {}

    ::RecipeUnlockingRequirement serialize() const {
        ::RecipeUnlockingRequirement result;
        for (auto& item : mItems) {
            result.mIngredients->push_back(item.pImpl->serialize());
        }
        result.mContext = RecipeUnlockingRequirement::UnlockingContext((int)mContext);
        return result;
    }
};

class CustomShapedRecipeBase : public ::ShapedRecipe {
public:
    CustomShapedRecipeBase(
        std::string_view                             craftingTag,
        std::string_view                             recipeId,
        std::vector<std::string> const&              shape,
        ICustomShapedRecipe::ShapedIngredients const ingredients,
        ::ItemInstance const&                        result,
        int                                          priority,
        ICustomRecipe::UnlockingRequirement const    unlock,
        bool                                         assumeSymmetry
    );
};

class CustomShapedMultiRecipeBase : public CustomShapedRecipeBase {
    std::function<void(std::unordered_map<std::string, std::vector<::ItemStack const*>>, ::ItemInstance& output)>
                                           mCraftingFunction;
    ICustomShapedRecipe::ShapedIngredients mIngredients;
    std::vector<ItemInstance>              mCraftingResult;
    mutable std::vector<ItemInstance>      mRuntimeResult;

public:
    CustomShapedMultiRecipeBase(
        std::string_view                             craftingTag,
        std::string_view                             recipeId,
        std::vector<std::string> const&              shape,
        ICustomShapedRecipe::ShapedIngredients const ingredients,
        ::ItemInstance const&                        result,
        int                                          priority,
        ICustomRecipe::UnlockingRequirement const    unlock,
        bool                                         assumeSymmetry,
        std::function<void(std::unordered_map<std::string, std::vector<::ItemStack const*>>, ::ItemInstance& output)>&&
            craftingFunction
    );

    ::std::vector<::ItemInstance> const& assemble(::CraftingContainer&, ::CraftingContext&) const override;

    ::std::vector<::ItemInstance> const& getResultItems() const override;
};

class CustomShapelessRecipeBase : public ::ShapelessRecipe {
public:
    CustomShapelessRecipeBase(
        std::string_view                             craftingTag,
        std::string_view                             recipeId,
        std::vector<ICustomRecipe::Ingredient> const ingredients,
        ::ItemInstance const&                        result,
        int                                          priority,
        ICustomRecipe::UnlockingRequirement const    unlock
    );
};

class CustomShapelessMultiRecipeBase : public CustomShapelessRecipeBase {
    std::function<void(std::vector<::ItemStack const*>, ::ItemInstance& output)> mCraftingFunction;
    std::vector<ItemInstance>                                                    mCraftingResult;
    mutable std::vector<ItemInstance>                                            mRuntimeResult;

public:
    CustomShapelessMultiRecipeBase(
        std::string_view                                                               craftingTag,
        std::string_view                                                               recipeId,
        std::vector<ICustomRecipe::Ingredient> const                                   ingredients,
        ::ItemInstance const&                                                          result,
        int                                                                            priority,
        ICustomRecipe::UnlockingRequirement const                                      unlock,
        std::function<void(std::vector<::ItemStack const*>, ::ItemInstance& output)>&& craftingFunction
    );

    ::std::vector<::ItemInstance> const& assemble(::CraftingContainer&, ::CraftingContext&) const override;

    ::std::vector<::ItemInstance> const& getResultItems() const override;
};

} // namespace modapi::inline recipe
