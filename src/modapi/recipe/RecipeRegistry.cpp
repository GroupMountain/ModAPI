#include "modapi/recipe/RecipeRegistry.h"
#include "modapi/core/FileUtils.h"
#include "modapi/core/RegistryEvent.h"
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
#include <atomic>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/server/ServerStartedEvent.h>
#include <ll/api/memory/Hook.h>
#include <ll/api/service/Bedrock.h>
#include <mc/deps/core/sem_ver/SemVersionConstant.h>
#include <mc/deps/json/Reader.h>
#include <mc/deps/json/Value.h>
#include <mc/network/packet/CraftingDataPacket.h>
#include <mc/network/packet/MaterialReducerEntryOutput.h>
#include <mc/resources/MinEngineVersion.h>
#include <mc/world/actor/player/Player.h>
#include <mc/world/inventory/network/crafting/RecipeNetIdTag.h>
#include <mc/world/item/SortItemInstanceIdAux.h>
#include <mc/world/item/crafting/Recipe.h>
#include <mc/world/item/crafting/RecipeType.h>
#include <mc/world/item/crafting/Recipes.h>
#include <mc/world/level/Level.h>
#include <nlohmann/json.hpp>

namespace {

// 26.51 removed `Recipes::loadRecipe(pair<string, Json::Value> const&, ...)` and split it into the
// key -> `RecipeType` lookup plus `Recipes::_loadRecipe(...)`. This mirrors the lookup table the
// vanilla recipe loader uses (BDS 1.26.51.1: a `{char const*, size_t, RecipeType}` table, one entry
// per key, in `RecipeType` order). The caller keeps passing the short `recipe_*` form.
::RecipeType recipeTypeFromKey(std::string_view key) {
    constexpr std::string_view prefix = "minecraft:";
    if (key.starts_with(prefix)) key.remove_prefix(prefix.size());
    if (key == "recipe_shaped") return ::RecipeType::Shaped;
    if (key == "recipe_shapeless") return ::RecipeType::Shapeless;
    if (key == "recipe_furnace") return ::RecipeType::Furnace;
    if (key == "recipe_brewing_mix") return ::RecipeType::BrewingMix;
    if (key == "recipe_brewing_container") return ::RecipeType::BrewingContainer;
    if (key == "recipe_material_reduction") return ::RecipeType::MaterialReduction;
    if (key == "recipe_smithing_transform") return ::RecipeType::SmithingTransform;
    if (key == "recipe_smithing_trim") return ::RecipeType::SmithingTrim;
    return ::RecipeType::Invalid;
}

// `_loadRecipe` takes the recipe id and type as separate arguments now; the id is the
// `description.identifier` it reads itself.
bool loadRecipeFromJson(
    ::Recipes&                recipes,
    ::RecipeType              type,
    ::Json::Value const&      objData,
    ::MinEngineVersion const& minEngineVersion,
    ::SemVersion const&       formatVersion,
    bool                      isBaseGamePack
) {
    if (type == ::RecipeType::Invalid) return false;
    auto const& description = objData["description"];
    if (!description.isObject()) return false;
    auto const& identifier = description["identifier"];
    if (!identifier.isString()) return false;
    // `_loadRecipe` reuses the caller's scratch buffers between recipes, so they have to outlive
    // the call. Recipe loading is single threaded.
    static ::Recipes::Buffers buffers;
    return recipes
        ._loadRecipe(identifier.asString(""), type, objData, minEngineVersion, formatVersion, isBaseGamePack, buffers);
}

// Looks a recipe up among the ones a `Recipes` instance holds; used to report the product of
// registrations that only yield a recipe id (the JSON loader).
::Recipe* findRegisteredRecipe(::Recipes& recipes, std::string const& recipeId) {
    for (auto& [recipeTag, recipeMap] : *recipes.mRecipes) {
        if (auto it = recipeMap.find(recipeId); it != recipeMap.end() && it->second) {
            return it->second.get();
        }
    }
    return nullptr;
}

} // namespace

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

struct RecipeRegistry::Impl {
    // Bound by the hook below, once the vanilla recipe pass has run for a level.
    ::Recipes*                                     mCurrentRecipes = nullptr;
    bool                                           mReady          = false;
    ll::memory::HookRegistrar<RegisterRecipesHook> mHook;

    ::SemVersion const& getFormatVersion() const { return CurrentSemVersion; }

    ::MinEngineVersion const& getMinEngineVersion() const {
        static auto min_engine_version = ::MinEngineVersion::fromString(getFormatVersion().asString());
        return min_engine_version;
    }
};

// `ServerLevel::initialize` calls this once per loaded level; recipes live in that level's Recipes
// object, so custom recipes are registered for every instance that is initialised.
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
    RecipeRegistry::getInstance()._bindRegistry(*this);
}

RecipeRegistry::RecipeRegistry() : pImpl(::std::make_unique<Impl>()) {}

RecipeRegistry& RecipeRegistry::getInstance() {
    // Deliberately leaked: the hook registrar this instance owns must not unregister its hooks
    // while the process/DLL is being torn down.
    static RecipeRegistry& instance = *new RecipeRegistry();
    return instance;
}

bool RecipeRegistry::isReady() const noexcept { return pImpl->mReady; }

void RecipeRegistry::ensureEventRegistered() {
    // The vanilla hooks live in the registry's implementation, so the singleton has to exist before
    // the pass that fires them; every consumer goes through this entry point.
    (void)getInstance();
    static std::atomic_bool registered = false;
    core::ensureRegistryEventRegistered<EventType>(registered);
}

void RecipeRegistry::_bindRegistry(::Recipes& recipes) {
    pImpl->mCurrentRecipes = &recipes;
    pImpl->mReady          = true;
    ll::event::EventBus::getInstance().publish(RecipeReadyEvent{*this});
}

RecipeRegistry::ProductRef RecipeRegistry::_registerRecipe(std::function<std::unique_ptr<ICustomRecipe>()>&& func) {
    if (pImpl->mCurrentRecipes == nullptr) {
        core::getLogger().error(
            "RecipeRegistry: recipes can only be registered during the vanilla recipe pass; "
            "use DeferredRegister<RecipeRegistry, ...> to register recipes before the server starts."
        );
        return {};
    }
    try {
        auto& recipes      = *pImpl->mCurrentRecipes;
        auto  customRecipe = func();
        customRecipe->_init();
        if (customRecipe->pImpl->mFurnaceRecipe) {
            customRecipe->pImpl->mFurnaceRecipe->registerRecipe(recipes);
            return {};
        }
        if (customRecipe->pImpl->mBrewingRecipe) {
            customRecipe->pImpl->mBrewingRecipe->registerRecipe();
            return {};
        }
        // The first recipe of the custom recipe is the product; the object stays alive in the
        // Recipes instance it was moved into.
        ::Recipe* firstRecipe = nullptr;
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
            if (firstRecipe == nullptr) firstRecipe = recipe.get();
            recipes._addItemRecipe(::std::move(recipe));
        }
        return firstRecipe;
    } catch (...) {
        return {};
    }
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

RecipeRegistry::ProductRef RecipeRegistry::registerShapedRecipe(
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

RecipeRegistry::ProductRef RecipeRegistry::registerShapedMultiRecipe(
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

RecipeRegistry::ProductRef RecipeRegistry::registerShapelessRecipe(
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

RecipeRegistry::ProductRef RecipeRegistry::registerShapelessMultiRecipe(
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

RecipeRegistry::ProductRef RecipeRegistry::registerStoneCutterRecipe(
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

RecipeRegistry::ProductRef RecipeRegistry::registerFurnaceRecipe(
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

RecipeRegistry::ProductRef RecipeRegistry::registerBrewingRecipe(
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

RecipeRegistry::ProductRef RecipeRegistry::registerSmithingTransformRecipe(
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

RecipeRegistry::ProductRef RecipeRegistry::registerSmithingTrimRecipe(
    std::string const&               recipeId,
    ICustomRecipe::Ingredient const& smithingTemplate,
    ICustomRecipe::Ingredient const& baseIngredient,
    ICustomRecipe::Ingredient const& additionIngredient
) {
    return registerRecipe<CustomSmithingTrimRecipe>(recipeId, smithingTemplate, baseIngredient, additionIngredient);
}

bool RecipeRegistry::unregisterRecipe(std::string const& recipeId, bool updateClients) {
    bool result = false;
    auto level  = ll::service::getLevel();
    if (!level) return false;

    auto& recipes = level->getRecipes();
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

void RecipeRegistry::updateClientRecipes() {
    auto level = ll::service::getLevel();
    if (!level) return;

    ::CraftingDataPacket packet(::CraftingDataPacketPayload::fromRecipes(level->getRecipes(), false));
    level->forEachPlayer([&packet](::Player& player) -> bool {
        player.sendNetworkPacket(packet);
        return true;
    });
}

RecipeRegistry::ProductRef RecipeRegistry::registerRecipeFromMemoryJson(std::string const& rawJson) {
    if (pImpl->mCurrentRecipes == nullptr) {
        core::getLogger().error(
            "RecipeRegistry: recipes can only be registered during the vanilla recipe pass; "
            "use DeferredRegister<RecipeRegistry, ...> to register recipes before the server starts."
        );
        return {};
    }

    ::Recipe* firstRecipe = nullptr;
    try {
        auto json = ::nlohmann::json::parse(rawJson, nullptr, true, true);
        for (auto& info : json.items()) {
            if (!info.key().starts_with("minecraft:recipe_") || !info.value().is_object()) continue;

            auto                  data = info.value().dump();
            static ::Json::Reader reader;
            ::Json::Value         value;
            reader.parse(data, value, true);
            try {
                if (!loadRecipeFromJson(
                        *pImpl->mCurrentRecipes,
                        recipeTypeFromKey(info.key()),
                        value,
                        pImpl->getMinEngineVersion(),
                        pImpl->getFormatVersion(),
                        true
                    )) {
                    continue;
                }
                if (firstRecipe == nullptr) {
                    auto const& identifier = value["description"]["identifier"];
                    if (identifier.isString()) {
                        firstRecipe = findRegisteredRecipe(*pImpl->mCurrentRecipes, identifier.asString(""));
                    }
                }
            } catch (...) {}
        }
    } catch (...) {}
    return firstRecipe;
}

RecipeRegistry::ProductRef RecipeRegistry::registerRecipeFromJsonFile(std::filesystem::path const& jsonPath) {
    if (std::filesystem::is_regular_file(jsonPath)) {
        if (auto content = core::readFile(jsonPath)) {
            return registerRecipeFromMemoryJson(*content);
        }
    }
    return {};
}

RecipeRegistry& RecipeReadyEvent::registry() const { return mRegistry; }

} // namespace modapi::inline recipe