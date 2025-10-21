#include "modapi/recipe/CustomRecipeRegistry.h"
#include "modapi/recipe/SharedTypes.h"
#include "modapi/recipe/base/ICustomBrewingRecipe.h"
#include "modapi/recipe/base/ICustomFurnaceRecipe.h"
#include "modapi/recipe/base/ICustomRecipe.h"
#include "modapi/recipe/base/ICustomShapedMultiRecipe.h"
#include "modapi/recipe/base/ICustomShapedRecipe.h"
#include "modapi/recipe/base/ICustomShapelessMultiRecipe.h"
#include "modapi/recipe/base/ICustomShapelessRecipe.h"
#include "modapi/recipe/base/ICustomSmithingTransformRecipe.h"
#include "modapi/recipe/base/ICustomSmithingTrimRecipe.h"
#include "modapi/recipe/base/ICustomStoneCutterRecipe.h"
#include <gmlib/gm/io/FileUtils.h>
#include <gmlib/mc/world/Level.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/server/ServerStartedEvent.h>
#include <ll/api/memory/Hook.h>
#include <ll/api/service/Bedrock.h>
#include <mc/deps/core/sem_ver/SemVersionConstant.h>
#include <mc/deps/json/Reader.h>
#include <mc/deps/json/Value.h>
#include <mc/network/packet/CraftingDataPacket.h>
#include <mc/resources/MinEngineVersion.h>
#include <mc/world/inventory/network/crafting/RecipeNetIdTag.h>
#include <mc/world/item/SortItemInstanceIdAux.h>
#include <mc/world/item/crafting/Recipe.h>
#include <mc/world/level/Level.h>
#include <nlohmann/json.hpp>

namespace std {

template <>
class hash<RecipeNetId> {
public:
    size_t operator()(RecipeNetId const& netId) const { return std::hash<uint>{}(netId.mRawId); }
};

} // namespace std

bool operator==(RecipeNetId const& lhs, RecipeNetId const& rhs) { return lhs.mRawId == rhs.mRawId; }

namespace modapi::inline recipe {

static auto CurrentSemVersion = ::SemVersion(SharedConstants::CurrentGameSemVersion());

struct RegisterRecipesHook;

struct CustomRecipeRegistry::Impl {
    struct PendingPendingJsonRecipe {
        std::string mType;
        std::string mData;
    };

    ::Recipes*                                                   mRecipesRegistry;
    std::vector<PendingPendingJsonRecipe>                        mPendingJsonRecipes;
    std::vector<std::function<std::unique_ptr<ICustomRecipe>()>> mPendingRecipes;
    ll::memory::HookRegistrar<RegisterRecipesHook>               mHook;

    ::SemVersion const& getFormatVersion() const { return CurrentSemVersion; }

    ::MinEngineVersion const& getMinEngineVersion() const {
        static auto min_engine_version = ::MinEngineVersion::fromString(getFormatVersion().asString());
        return min_engine_version;
    }
};

LL_TYPE_INSTANCE_HOOK(
    RegisterRecipesHook,
    HookPriority::Normal,
    ::Recipes,
    &::Recipes::init,
    void,
    ::ResourcePackManager&   resourcePackManager,
    ::ExternalRecipeStore&   recipeStorage,
    ::BaseGameVersion const& baseGameVersion,
    ::Experiments const&     experiments
) {
    origin(resourcePackManager, recipeStorage, baseGameVersion, experiments);
    auto& manager            = *CustomRecipeRegistry::getInstance().pImpl;
    manager.mRecipesRegistry = this;
    static ::Json::Reader reader;
    for (auto const& recipe : manager.mPendingJsonRecipes) {
        ::Json::Value value;
        reader.parse(recipe.mData, value, true);
        try {
            loadRecipe({recipe.mType, value}, manager.getMinEngineVersion(), manager.getFormatVersion(), true);
        } catch (...) {}
    }
    for (auto& func : manager.mPendingRecipes) {
        try {
            auto customRecipe = func();
            customRecipe->_init();
            if (customRecipe->pImpl->mFurnaceRecipe) {
                customRecipe->pImpl->mFurnaceRecipe->registerRecipe(*this);
            } else if (customRecipe->pImpl->mBrewingRecipe) {
                customRecipe->pImpl->mBrewingRecipe->registerRecipe();
            } else {
                for (auto& recipe : customRecipe->pImpl->mRecipes) {
                    auto recipeId = *recipe->mRecipeId;
                    for (auto it = mUnlockableRecipes->begin(); it != mUnlockableRecipes->end(); ++it) {
                        if (*((*it)->mRecipeId) == recipeId) {
                            mUnlockableRecipes->erase(it);
                            --it;
                        }
                    }
                    for (auto it = mRecipesByNetId->begin(); it != mRecipesByNetId->end(); ++it) {
                        if (*((*it).second->mRecipeId) == recipeId) {
                            mRecipesByNetId->erase(it);
                            --it;
                        }
                    }
                    for (auto& [item, recipeMap] : *mRecipesByOutput) {
                        if (recipeMap.contains(recipeId)) {
                            recipeMap.erase(recipeId);
                        }
                    }
                    for (auto& [recipeTag, recipeMap] : *mRecipes) {
                        if (recipeMap.contains(recipeId)) {
                            recipeMap.erase(recipeId);
                        }
                    }
                    _addItemRecipe(::std::move(recipe));
                }
            }
        } catch (...) {}
    }
    manager.mPendingRecipes.clear();
}

CustomRecipeRegistry::CustomRecipeRegistry() : pImpl(::std::make_unique<Impl>()) {}

CustomRecipeRegistry& CustomRecipeRegistry::_registerRecipe(std::function<std::unique_ptr<ICustomRecipe>()>&& func) {
    if (pImpl->mRecipesRegistry) {
        try {
            auto& recipes      = *pImpl->mRecipesRegistry;
            auto  customRecipe = func();
            customRecipe->_init();
            if (customRecipe->pImpl->mFurnaceRecipe) {
                customRecipe->pImpl->mFurnaceRecipe->registerRecipe(recipes);
            } else if (customRecipe->pImpl->mBrewingRecipe) {
                customRecipe->pImpl->mBrewingRecipe->registerRecipe();
            } else {
                for (auto& recipe : customRecipe->pImpl->mRecipes) {
                    auto recipeId = *recipe->mRecipeId;
                    for (auto it = recipes.mUnlockableRecipes->begin(); it != recipes.mUnlockableRecipes->end(); ++it) {
                        if (*((*it)->mRecipeId) == recipeId) {
                            recipes.mUnlockableRecipes->erase(it);
                            --it;
                        }
                    }
                    for (auto it = recipes.mRecipesByNetId->begin(); it != recipes.mRecipesByNetId->end(); ++it) {
                        if (*((*it).second->mRecipeId) == recipeId) {
                            recipes.mRecipesByNetId->erase(it);
                            --it;
                        }
                    }
                    for (auto& [item, recipeMap] : *recipes.mRecipesByOutput) {
                        if (recipeMap.contains(recipeId)) {
                            recipeMap.erase(recipeId);
                        }
                    }
                    for (auto& [recipeTag, recipeMap] : *recipes.mRecipes) {
                        if (recipeMap.contains(recipeId)) {
                            recipeMap.erase(recipeId);
                        }
                    }
                    recipes._addItemRecipe(::std::move(recipe));
                }
            }
        } catch (...) {}
    } else {
        pImpl->mPendingRecipes.push_back(std::move(func));
    }
    return *this;
}

CustomRecipeRegistry& CustomRecipeRegistry::getInstance() {
    static CustomRecipeRegistry instance;
    if (!instance.pImpl->mRecipesRegistry) {
        if (auto level = ll::service::getLevel()) {
            instance.pImpl->mRecipesRegistry = &level->getRecipes();
        }
    }
    return instance;
}

class CustomShapedRecipe : public ICustomShapedRecipe {
    std::string              mRecipeId;
    std::vector<std::string> mShape;
    ShapedIngredients        mIngredients;
    ::ItemInstance           mResult;
    UnlockingRequirement     mUnlock;
    std::vector<std::string> mTags;
    bool                     mAssumeSymmetry;
    int                      mPriority;

public:
    CustomShapedRecipe(
        std::string const&                            recipeId,
        std::vector<std::string> const&               shape,
        ICustomShapedRecipe::ShapedIngredients const& ingredients,
        ::ItemInstance const&                         result,
        ICustomRecipe::UnlockingRequirement const&    unlock,
        std::vector<std::string> const&               tags,
        bool                                          assumeSymmetry,
        int                                           priority
    )
    : ICustomShapedRecipe(),
      mRecipeId(recipeId),
      mShape(shape),
      mIngredients(ingredients),
      mResult(result),
      mUnlock(unlock),
      mTags(tags),
      mAssumeSymmetry(assumeSymmetry),
      mPriority(priority) {}

    std::string getRecipeId() const override { return mRecipeId; }

    std::vector<::std::string> getCraftingTags() const override { return mTags; }

    std::vector<std::string> getShape() const override { return mShape; }

    ShapedIngredients getIngredients() const override { return mIngredients; }

    ::ItemInstance getResult() const override { return mResult; }

    int getPriority() const override { return mPriority; }

    UnlockingRequirement getUnlockingRequirement() const override { return mUnlock; }

    bool isAssumeSymmetry() const override { return mAssumeSymmetry; }
};

CustomRecipeRegistry& CustomRecipeRegistry::registerShapedRecipe(
    std::string const&                            recipeId,
    std::vector<std::string> const&               shape,
    ICustomShapedRecipe::ShapedIngredients const& ingredients,
    ::ItemInstance const&                         result,
    ICustomRecipe::UnlockingRequirement const&    unlock,
    std::vector<std::string> const&               tags,
    bool                                          assumeSymmetry,
    int                                           priority
) {
    return registerRecipe<
        CustomShapedRecipe>(recipeId, shape, ingredients, result, unlock, tags, assumeSymmetry, priority);
}

class CustomShapedMultiRecipe : public ICustomShapedMultiRecipe {
    std::string              mRecipeId;
    std::vector<std::string> mShape;
    ShapedIngredients        mIngredients;
    ::ItemInstance           mResult;
    CraftingCallback         mCraftingCallback;
    UnlockingRequirement     mUnlock;
    std::vector<std::string> mTags;
    bool                     mAssumeSymmetry;
    int                      mPriority;

public:
    CustomShapedMultiRecipe(
        std::string const&                            recipeId,
        std::vector<std::string> const&               shape,
        ICustomShapedRecipe::ShapedIngredients const& ingredients,
        ::ItemInstance const&                         result,
        CraftingCallback const&                       craftingCallback,
        ICustomRecipe::UnlockingRequirement const&    unlock,
        std::vector<std::string> const&               tags,
        bool                                          assumeSymmetry,
        int                                           priority
    )
    : ICustomShapedMultiRecipe(),
      mRecipeId(recipeId),
      mShape(shape),
      mIngredients(ingredients),
      mResult(result),
      mCraftingCallback(std::move(craftingCallback)),
      mUnlock(unlock),
      mTags(tags),
      mAssumeSymmetry(assumeSymmetry),
      mPriority(priority) {}

    std::string getRecipeId() const override { return mRecipeId; }

    std::vector<::std::string> getCraftingTags() const override { return mTags; }

    std::vector<std::string> getShape() const override { return mShape; }

    ShapedIngredients getIngredients() const override { return mIngredients; }

    ::ItemInstance getResult() const override { return mResult; }

    int getPriority() const override { return mPriority; }

    UnlockingRequirement getUnlockingRequirement() const override { return mUnlock; }

    bool isAssumeSymmetry() const override { return mAssumeSymmetry; }

    CraftingCallback getCraftingCallback() const override { return mCraftingCallback; }
};

CustomRecipeRegistry& CustomRecipeRegistry::registerShapedMultiRecipe(
    std::string const&                            recipeId,
    std::vector<std::string> const&               shape,
    ICustomShapedRecipe::ShapedIngredients const& ingredients,
    ::ItemInstance const&                         result,
    ICustomShapedMultiRecipe::CraftingCallback&&  craftingCallback,
    ICustomRecipe::UnlockingRequirement const&    unlock,
    std::vector<std::string> const&               tags,
    bool                                          assumeSymmetry,
    int                                           priority
) {
    return registerRecipe<CustomShapedMultiRecipe>(
        recipeId,
        shape,
        ingredients,
        result,
        std::move(craftingCallback),
        unlock,
        tags,
        assumeSymmetry,
        priority
    );
}

class CustomShapelessRecipe : public ICustomShapelessRecipe {
    std::string              mRecipeId;
    std::vector<Ingredient>  mIngredients;
    ::ItemInstance           mResult;
    UnlockingRequirement     mUnlock;
    std::vector<std::string> mTags;
    int                      mPriority;

public:
    CustomShapelessRecipe(
        std::string const&                         recipeId,
        std::vector<Ingredient> const&             ingredients,
        ::ItemInstance const&                      result,
        ICustomRecipe::UnlockingRequirement const& unlock,
        std::vector<std::string> const&            tags,
        int                                        priority
    )
    : ICustomShapelessRecipe(),
      mRecipeId(recipeId),
      mIngredients(ingredients),
      mResult(result),
      mUnlock(unlock),
      mTags(tags),
      mPriority(priority) {}

    std::string getRecipeId() const override { return mRecipeId; }

    std::vector<::std::string> getCraftingTags() const override { return mTags; }

    std::vector<Ingredient> getIngredients() const override { return mIngredients; }

    ::ItemInstance getResult() const override { return mResult; }

    int getPriority() const override { return mPriority; }

    UnlockingRequirement getUnlockingRequirement() const override { return mUnlock; }
};

CustomRecipeRegistry& CustomRecipeRegistry::registerShapelessRecipe(
    std::string const&                            recipeId,
    std::vector<ICustomRecipe::Ingredient> const& ingredients,
    ::ItemInstance const&                         result,
    ICustomRecipe::UnlockingRequirement const&    unlock,
    std::vector<std::string> const&               tags,
    int                                           priority
) {
    return registerRecipe<CustomShapelessRecipe>(recipeId, ingredients, result, unlock, tags, priority);
}

class CustomShapelessMultiRecipe : public ICustomShapelessMultiRecipe {
    std::string              mRecipeId;
    std::vector<Ingredient>  mIngredients;
    ::ItemInstance           mResult;
    CraftingCallback         mCraftingCallback;
    UnlockingRequirement     mUnlock;
    std::vector<std::string> mTags;
    int                      mPriority;

public:
    CustomShapelessMultiRecipe(
        std::string const&                            recipeId,
        std::vector<ICustomRecipe::Ingredient> const& ingredients,
        ::ItemInstance const&                         result,
        CraftingCallback const&                       craftingCallback,
        ICustomRecipe::UnlockingRequirement const&    unlock,
        std::vector<std::string> const&               tags,
        int                                           priority
    )
    : ICustomShapelessMultiRecipe(),
      mRecipeId(recipeId),
      mIngredients(ingredients),
      mResult(result),
      mCraftingCallback(std::move(craftingCallback)),
      mUnlock(unlock),
      mTags(tags),
      mPriority(priority) {}

    std::string getRecipeId() const override { return mRecipeId; }

    std::vector<::std::string> getCraftingTags() const override { return mTags; }

    std::vector<Ingredient> getIngredients() const override { return mIngredients; }

    ::ItemInstance getResult() const override { return mResult; }

    int getPriority() const override { return mPriority; }

    UnlockingRequirement getUnlockingRequirement() const override { return mUnlock; }

    CraftingCallback getCraftingCallback() const override { return mCraftingCallback; }
};

CustomRecipeRegistry& CustomRecipeRegistry::registerShapelessMultiRecipe(
    std::string const&                              recipeId,
    std::vector<ICustomRecipe::Ingredient> const&   ingredients,
    ::ItemInstance const&                           result,
    ICustomShapelessMultiRecipe::CraftingCallback&& craftingCallback,
    ICustomRecipe::UnlockingRequirement const&      unlock,
    std::vector<std::string> const&                 tags,
    int                                             priority
) {
    return registerRecipe<
        CustomShapelessMultiRecipe>(recipeId, ingredients, result, std::move(craftingCallback), unlock, tags, priority);
}

class CustomStoneCutterRecipe : public ICustomStoneCutterRecipe {
    std::string    mRecipeId;
    Ingredient     mIngredient;
    ::ItemInstance mResult;
    int            mPriority;

public:
    CustomStoneCutterRecipe(
        std::string const&    recipeId,
        Ingredient const&     ingredient,
        ::ItemInstance const& result,
        int                   priority
    )
    : ICustomStoneCutterRecipe(),
      mRecipeId(recipeId),
      mIngredient(ingredient),
      mResult(result),
      mPriority(priority) {}

    std::string getRecipeId() const override { return mRecipeId; }

    Ingredient getIngredient() const override { return mIngredient; }

    ::ItemInstance getResult() const override { return mResult; }

    int getPriority() const override { return mPriority; }
};

CustomRecipeRegistry& CustomRecipeRegistry::registerStoneCutterRecipe(
    std::string const&               recipeId,
    ICustomRecipe::Ingredient const& ingredient,
    ::ItemInstance const&            result,
    int                              priority
) {
    return registerRecipe<CustomStoneCutterRecipe>(recipeId, ingredient, result, priority);
}

class CustomFurnaceRecipe : public ICustomFurnaceRecipe {
    Ingredient               mInput;
    ::ItemInstance           mOutput;
    std::vector<std::string> mTags;

public:
    CustomFurnaceRecipe(Ingredient const& input, ::ItemInstance const& output, std::vector<::std::string> const& tags)
    : ICustomFurnaceRecipe(),
      mInput(input),
      mOutput(output),
      mTags(tags) {}

    std::vector<::std::string> getCraftingTags() const override { return mTags; }

    Ingredient getIngredient() const override { return mInput; }

    ::ItemInstance getResult() const override { return mOutput; }
};

CustomRecipeRegistry& CustomRecipeRegistry::registerFurnaceRecipe(
    ICustomRecipe::Ingredient const& input,
    ::ItemInstance const&            output,
    std::vector<std::string> const&  tags
) {
    return registerRecipe<CustomFurnaceRecipe>(input, output, tags);
}

class CustomBrewingRecipe : public ICustomBrewingRecipe {
    Ingredient mInput;
    Ingredient mReagent;
    Ingredient mOutput;

public:
    CustomBrewingRecipe(Ingredient const& input, Ingredient const& reagent, Ingredient const& output)
    : ICustomBrewingRecipe(),
      mInput(input),
      mReagent(reagent),
      mOutput(output) {}

    Ingredient getInput() const override { return mInput; }

    Ingredient getReagent() const override { return mReagent; }

    Ingredient getOutput() const override { return mOutput; }
};

CustomRecipeRegistry& CustomRecipeRegistry::registerBrewingRecipe(
    ICustomRecipe::Ingredient const& input,
    ICustomRecipe::Ingredient const& reagent,
    ICustomRecipe::Ingredient const& output
) {
    return registerRecipe<CustomBrewingRecipe>(input, reagent, output);
}

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

CustomRecipeRegistry& CustomRecipeRegistry::registerSmithingTransformRecipe(
    std::string const&               recipeId,
    ICustomRecipe::Ingredient const& smithingTemplate,
    ICustomRecipe::Ingredient const& baseIngredient,
    ICustomRecipe::Ingredient const& additionIngredient,
    ::ItemInstance const&            result
) {
    return registerRecipe<CustomSmithingTransformRecipe>(
        recipeId,
        smithingTemplate,
        baseIngredient,
        additionIngredient,
        result
    );
}

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

CustomRecipeRegistry& CustomRecipeRegistry::registerSmithingTrimRecipe(
    std::string const&               recipeId,
    ICustomRecipe::Ingredient const& smithingTemplate,
    ICustomRecipe::Ingredient const& baseIngredient,
    ICustomRecipe::Ingredient const& additionIngredient
) {
    return registerRecipe<CustomSmithingTrimRecipe>(recipeId, smithingTemplate, baseIngredient, additionIngredient);
}

bool CustomRecipeRegistry::unregisterRecipe(std::string const& recipeId, bool updateClients) {
    bool  result  = false;
    auto& recipes = ll::service::getLevel()->getRecipes();
    for (auto it = recipes.mUnlockableRecipes->begin(); it != recipes.mUnlockableRecipes->end(); ++it) {
        if (*((*it)->mRecipeId) == recipeId) {
            recipes.mUnlockableRecipes->erase(it);
            --it;
        }
    }
    for (auto it = recipes.mRecipesByNetId->begin(); it != recipes.mRecipesByNetId->end(); ++it) {
        if (*((*it).second->mRecipeId) == recipeId) {
            recipes.mRecipesByNetId->erase(it);
            --it;
        }
    }
    for (auto& [item, recipeMap] : *recipes.mRecipesByOutput) {
        if (recipeMap.contains(recipeId)) {
            recipeMap.erase(recipeId);
        }
    }
    for (auto& [recipeTag, recipeMap] : *recipes.mRecipes) {
        if (recipeMap.contains(recipeId)) {
            recipeMap.erase(recipeId);
            result = true;
        }
    }
    if (updateClients && result) {
        updateClientRecipes();
    }
    return result;
}

void CustomRecipeRegistry::updateClientRecipes() {
    auto level = gmlib::GMLevel::getInstance();
    level->sendPacketRawToClients(*::CraftingDataPacket::prepareFromRecipes(level->getRecipes(), false));
}

CustomRecipeRegistry& CustomRecipeRegistry::registerRecipeFromMemoryJson(std::string const& rawJson) {
    try {
        auto json = ::nlohmann::json::parse(rawJson, nullptr, true, true);
        for (auto& info : json.items()) {
            if (info.key().starts_with("minecraft:recipe_") && info.value().is_object()) {
                auto type = ll::string_utils::replaceAll(info.key(), "minecraft:", "");
                auto data = info.value().dump();
                if (pImpl->mRecipesRegistry) {
                    auto&                 recipes = ll::service::getLevel()->getRecipes();
                    static ::Json::Reader reader;
                    ::Json::Value         value;
                    reader.parse(data, value, true);
                    try {
                        recipes
                            .loadRecipe({type, value}, pImpl->getMinEngineVersion(), pImpl->getFormatVersion(), true);
                    } catch (...) {}
                } else {
                    pImpl->mPendingJsonRecipes.emplace_back(type, data);
                }
            }
        }
    } catch (...) {}
    return *this;
}

CustomRecipeRegistry& CustomRecipeRegistry::registerRecipeFromJsonFile(std::filesystem::path const& jsonPath) {
    if (std::filesystem::is_regular_file(jsonPath)) {
        if (auto content = gmlib::file_utils::readFile(jsonPath)) {
            return registerRecipeFromMemoryJson(*content);
        }
    }
    return *this;
}

} // namespace modapi::inline recipe