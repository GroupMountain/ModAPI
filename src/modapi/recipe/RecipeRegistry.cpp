#include "modapi/recipe/RecipeRegistry.h"
#include "modapi/core/FileUtils.h"
#include "modapi/core/RegistryEvent.h"
#include "modapi/recipe/SharedTypes.h"
#include <atomic>
#include <ll/api/event/EventBus.h>
#include <ll/api/memory/Hook.h>
#include <ll/api/service/Bedrock.h>
#include <mc/deps/core/sem_ver/SemVersionConstant.h>
#include <mc/deps/json/Reader.h>
#include <mc/deps/json/Value.h>
#include <mc/network/packet/CraftingDataPacket.h>
#include <mc/resources/MinEngineVersion.h>
#include <mc/world/actor/player/Player.h>
#include <mc/world/inventory/network/crafting/RecipeNetIdTag.h>
#include <mc/world/item/crafting/Recipe.h>
#include <mc/world/item/crafting/RecipeType.h>
#include <mc/world/item/crafting/Recipes.h>
#include <mc/world/level/Level.h>
#include <nlohmann/json.hpp>

namespace {

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
    static ::Recipes::Buffers buffers;
    return recipes
        ._loadRecipe(identifier.asString(""), type, objData, minEngineVersion, formatVersion, isBaseGamePack, buffers);
}

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
    ::Recipes*                                     mCurrentRecipes = nullptr;
    bool                                           mReady          = false;
    ll::memory::HookRegistrar<RegisterRecipesHook> mHook;

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
    RecipeRegistry::getInstance()._bindRegistry(*this);
}

RecipeRegistry::RecipeRegistry() : pImpl(::std::make_unique<Impl>()) {}

RecipeRegistry& RecipeRegistry::getInstance() {
    static RecipeRegistry& instance = *new RecipeRegistry();
    return instance;
}

bool RecipeRegistry::isReady() const noexcept { return pImpl->mReady; }

void RecipeRegistry::ensureEventRegistered() {
    (void)getInstance();
    static std::atomic_bool registered = false;
    core::ensureRegistryEventRegistered<EventType>(registered);
}

void RecipeRegistry::_bindRegistry(::Recipes& recipes) {
    pImpl->mCurrentRecipes = &recipes;
    pImpl->mReady          = true;
    ll::event::EventBus::getInstance().publish(RecipeReadyEvent{*this});
}

RecipeRegistry::ProductRef RecipeRegistry::_registerRecipe(std::function<std::unique_ptr<ICustomRecipe>()>&& factory) {
    if (pImpl->mCurrentRecipes == nullptr) {
        core::getLogger().error(
            "RecipeRegistry: recipes can only be registered during the vanilla recipe pass; "
            "use DeferredRegister<RecipeRegistry, ...> to register recipes before the server starts."
        );
        return {};
    }
    try {
        auto& recipes      = *pImpl->mCurrentRecipes;
        auto  customRecipe = factory();
        if (!customRecipe) return {};
        customRecipe->_init();
        if (customRecipe->pImpl->mFurnaceRecipe) {
            customRecipe->pImpl->mFurnaceRecipe->registerRecipe(recipes);
            return {};
        }
        if (customRecipe->pImpl->mBrewingRecipe) {
            customRecipe->pImpl->mBrewingRecipe->registerRecipe();
            return {};
        }

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
                recipeMap.erase(recipeId);
            }
            for (auto& [recipeTag, recipeMap] : *recipes.mRecipes) {
                recipeMap.erase(recipeId);
            }
            if (firstRecipe == nullptr) firstRecipe = recipe.get();
            recipes._addItemRecipe(::std::move(recipe));
        }
        return firstRecipe;
    } catch (...) {
        return {};
    }
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
        recipeMap.erase(recipeId);
    }
    for (auto& [recipeTag, recipeMap] : *recipes.mRecipes) {
        if (recipeMap.erase(recipeId) > 0) result = true;
    }
    if (updateClients && result) updateClientRecipes();
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
        if (auto content = core::readFile(jsonPath)) return registerRecipeFromMemoryJson(*content);
    }
    return {};
}

RecipeRegistry& RecipeReadyEvent::registry() const { return mRegistry; }

} // namespace modapi::inline recipe
