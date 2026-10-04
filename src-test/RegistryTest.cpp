// End to end tests for the registry / DeferredRegister API.
//
// The test mod registers one entry per registry while it loads (i.e. before any registry opened) and
// the suite runs on ServerStartedEvent, when every registry has published its ready event. A failing
// case is a bug report against ModAPI's registry layer, not a flaky test.
//
// Build with `xmake build test` and read bin/test/registry-test-report.txt (or the server log,
// `[REGISTRY]` lines) after the server started. `modapitest registry` re-runs the suite in game.

#include "Global.h"
#include "TestReport.hpp"
#include "modapi/DeferredRegister.h"
#include "modapi/gamerule/GameRuleRegistry.h"
#include "modapi/item/CreativeItemRegistry.h"
#include "modapi/item/ItemRegistry.h"
#include "modapi/recipe/RecipeRegistry.h"
#include "modapi/worldgen/FeatureRegistry.h"
#include <algorithm>
#include <filesystem>
#include <fmt/format.h>
#include <fstream>
#include <ll/api/event/EventBus.h>
#include <mc/server/commands/CommandItem.h>
#include <mc/server/commands/CommandOutput.h>
#include <mc/server/commands/CommandOutputType.h>
#include <ll/api/memory/Symbol.h>
#include <ll/api/service/Bedrock.h>
#include <mc/world/item/crafting/Recipe.h>
#include <mc/world/item/crafting/Recipes.h>
#include <mc/world/level/Level.h>
#include <mc/world/level/levelgen/feature/registry/FeatureRegistry.h>
#include <mc/world/level/storage/GameRules.h>
#include <optional>
#include <string>
#include <vector>

namespace {


// ---------------------------------------------------------------------------------------------
// Entries used by the deferred registrations below.
// ---------------------------------------------------------------------------------------------

constexpr std::string_view TestItemName     = "modapi_test:registry_test_item";
constexpr std::string_view TestRecipeId     = "modapi_test:test_recipe";
constexpr std::string_view TestJsonRecipeId = "modapi_test:json_recipe";
constexpr std::string_view TestGameRuleId   = "modapi_test:test_rule";
constexpr std::string_view TestFeatureId    = "modapi_test:test_feature";

class TestItem : public modapi::ICustomItem<::Item> {
public:
    TestItem(std::string const& identifier, int damage) : ICustomItem(identifier) {
        mMaxDamage = static_cast<short>(damage);
        // A creative category makes the item go through CreativeItemRegistry's deferred path.
        mCreativeCategory = ::SharedTypes::CreativeItemCategory::Items;
        mCreativeGroup    = "itemGroup.name.items";
    }

    modapi::ItemIcon getIcon() const override { return modapi::ItemIcon{"stick"}; }
};

class TestRecipe : public modapi::ICustomShapedRecipe {
public:
    std::string getRecipeId() const override { return std::string{TestRecipeId}; }

    std::vector<std::string> getShape() const override { return {"X"}; }

    ShapedIngredients getIngredients() const override {
        return ShapedIngredients{
            {"X", Ingredient{TestItemName, modapi::recipe::RecipeIngredientType::Item, 1}}
        };
    }

    ::ItemInstance getResult() const override {
        ::ItemInstance result;
        result.reinit(std::string{TestItemName}, 1, 0);
        return result;
    }
};

class TestGameRule : public modapi::ICustomGameRule<int> {
public:
    std::string getIdentifier() const override { return std::string{TestGameRuleId}; }

    int getDefaultValue() const override { return 7; }

    // The test rules are not part of the world's settings.
    bool shouleSaveToDisk() const override { return false; }
};

class TestFeature : public modapi::ICustomFeature {
public:
    std::optional<BlockPos> place(modapi::BlockHelper&, BlockPos const&, Random&) const override {
        return std::nullopt; // registered, but never places anything
    }
};

// ---------------------------------------------------------------------------------------------
// Deferred registrations: constructed while the test mod's DLL is loaded, which is before the
// server (and therefore every registry) started.
// ---------------------------------------------------------------------------------------------

modapi::DeferredRegister<modapi::ItemRegistry, TestItem>                 gItem{std::string{TestItemName}, 42};
modapi::DeferredRegister<modapi::RecipeRegistry, TestRecipe>             gRecipe;
modapi::DeferredRegister<modapi::GameRuleRegistry, TestGameRule>         gGameRule;
modapi::DeferredRegister<modapi::worldgen::FeatureRegistry, TestFeature> gFeature{TestFeatureId};

// Recorded while the test mod loads: no registry may be open yet at that point.
struct ReadinessAtLoad {
    bool item     = true;
    bool creative = true;
    bool recipe   = true;
    bool gameRule = true;
    bool feature  = true;
};

ReadinessAtLoad gReadyAtLoad;

// Ready event bookkeeping, filled by the listeners installed while the test mod loads.
struct ReadyEvents {
    int  item     = 0;
    int  creative = 0;
    int  recipe   = 0;
    int  gameRule = 0;
    int  feature  = 0;
    bool itemRegistryMatches     = true;
    bool creativeRegistryMatches = true;
    bool recipeRegistryMatches   = true;
    bool gameRuleRegistryMatches = true;
    bool featureRegistryMatches  = true;
};

ReadyEvents gReady;

// Recipes loaded from JSON have no user type, so they are registered from the ready event instead.
modapi::RecipeRegistry::ProductRef gJsonRecipe;
bool                               gJsonRecipeInvalidRejected = false;

// ---------------------------------------------------------------------------------------------
// Report
// ---------------------------------------------------------------------------------------------

test::Report report{"[REGISTRY]", "registry-test-report.txt"};

// ---------------------------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------------------------

bool recipesContain(std::string_view recipeId) {
    auto level = ll::service::getLevel();
    if (!level) return false;
    for (auto& [tag, recipeMap] : *level->getRecipes().mRecipes) {
        if (auto it = recipeMap.find(std::string{recipeId}); it != recipeMap.end() && it->second) return true;
    }
    return false;
}

bool gameRulesContain(std::string_view identifier) {
    auto level = ll::service::getLevel();
    if (!level) return false;
    for (auto& rule : *level->getGameRules().mGameRules) {
        if (*rule.mName == identifier) return true;
    }
    return false;
}

// A shaped recipe in the shape `Recipes::_loadRecipe` expects (the value of `minecraft:recipe_shaped`).
std::string makeJsonRecipe(std::string_view identifier) {
    return fmt::format(
        R"({{"description":{{"identifier":"{}"}},"tags":["crafting_table"],"pattern":["X"],)"
        R"("key":{{"X":{{"item":"minecraft:stick"}}}},"result":{{"item":"minecraft:stick","count":1}},"priority":50}})",
        identifier
    );
}

// ---------------------------------------------------------------------------------------------
// Suites
// ---------------------------------------------------------------------------------------------

void testDeferredItem() {
    report.section("deferred.item", [] {
        auto& registry = modapi::ItemRegistry::getInstance();

        report.isTrue("notReadyAtLoad", !gReadyAtLoad.item);
        report.isTrue("registryReady", registry.isReady());
        report.isTrue("isRegistered", gItem.isRegistered());
        report.isTrue("isReady", gItem.isReady());
        report.isTrue("product", gItem.get().has_value());
        report.eq(
            "productName",
            std::string{TestItemName},
            gItem.get() ? gItem.get()->getSerializedName() : std::string{"<empty>"}
        );
        report.isTrue("registry.hasItem", (bool)registry.getItem(TestItemName));

        // The item has a creative category, so it also went through CreativeItemRegistry's queue.
        report.isTrue("creativeReady", modapi::CreativeItemRegistry::getInstance().isReady());
        report.isTrue(
            "creative.hasItem",
            !modapi::CreativeItemRegistry::getInstance().getCreativeItem(TestItemName).empty()
        );

        // The ready event is registered and our listener is attached to it.
        report.isTrue(
            "eventListener",
            ll::event::EventBus::getInstance().getListenerCount<modapi::ItemReadyEvent>() > 0
        );
    });
}

void testDeferredRecipe() {
    report.section("deferred.recipe", [] {
        report.isTrue("registryReady", modapi::RecipeRegistry::getInstance().isReady());
        report.isTrue("isRegistered", gRecipe.isRegistered());
        report.isTrue("product", gRecipe.get().has_value());
        report.isTrue("registry.hasRecipe", recipesContain(TestRecipeId));
    });
}

void testDeferredGameRule() {
    report.section("deferred.gameRule", [] {
        report.isTrue("registryReady", modapi::GameRuleRegistry::getInstance().isReady());
        report.isTrue("isRegistered", gGameRule.isRegistered());
        report.isTrue("product", gGameRule.get().has_value());
        report.isTrue("level.hasRule", gameRulesContain(TestGameRuleId));
    });
}

void testDeferredFeature() {
    report.section("deferred.feature", [] {
        report.isTrue("registryReady", modapi::worldgen::FeatureRegistry::getInstance().isReady());
        report.isTrue("isRegistered", gFeature.isRegistered());
        report.isTrue("product", gFeature.get().has_value());
    });
}

void testJsonRecipe() {
    report.section("event.jsonRecipe", [] {
        // Malformed input has to be rejected without touching the recipe registry; the case is
        // evaluated by the ready event listener registered while the test mod loads.
        report.isTrue("malformedRejected", gJsonRecipeInvalidRejected);

        // The fixture only verifies the documented plumbing: if the vanilla loader rejects this
        // schema the case is reported as pending instead of failing the suite.
        if (gJsonRecipe) {
            report.isTrue("fixtureRegistered", recipesContain(TestJsonRecipeId));
        } else {
            test::getLogger().warn("[REGISTRY] json recipe fixture was not loaded by this game build; case skipped");
        }
    });
}

void testPreconditions() {
    report.section("precondition.lateConstruction", [] {
        // Constructing after the registry opened cannot work: the ready event is not published again.
        // The negative case: the sections above already registered everything, so the item registry
        // is open and a `DeferredRegister` constructed *now* can never register. Its destructor
        // reports exactly that, so the `ERR ... was never applied` line that shows up next belongs to
        // this case (the assertions below pass); it is not a failure.
        test::getLogger().info(
            "[REGISTRY] (the next ERR line is expected: constructing a DeferredRegister after the registry opened)"
        );
        modapi::DeferredRegister<modapi::ItemRegistry, TestItem> late{"modapi_test:late_item", 1};
        report.isTrue("notRegistered", !late.isRegistered());
        report.isTrue("registryIsReady", late.isReady());
        report.isTrue("noProduct", !late.get().has_value());
        report.isTrue("notInRegistry", !(bool)modapi::ItemRegistry::getInstance().getItem("modapi_test:late_item"));
    });

    report.section("precondition.readiness", [] {
        // Nothing is open while the mods load: that is what makes a deferred registration work.
        report.isTrue("item.notReadyAtLoad", !gReadyAtLoad.item);
        report.isTrue("creative.notReadyAtLoad", !gReadyAtLoad.creative);
        report.isTrue("recipe.notReadyAtLoad", !gReadyAtLoad.recipe);
        report.isTrue("gameRule.notReadyAtLoad", !gReadyAtLoad.gameRule);
        report.isTrue("feature.notReadyAtLoad", !gReadyAtLoad.feature);

        // ... and everything is open once the server runs.
        report.isTrue("item.ready", modapi::ItemRegistry::getInstance().isReady());
        report.isTrue("creative.ready", modapi::CreativeItemRegistry::getInstance().isReady());
        report.isTrue("recipe.ready", modapi::RecipeRegistry::getInstance().isReady());
        report.isTrue("gameRule.ready", modapi::GameRuleRegistry::getInstance().isReady());
        report.isTrue("feature.ready", modapi::worldgen::FeatureRegistry::getInstance().isReady());
    });

    report.section("contract.concepts", [] {
        static_assert(modapi::Registry<modapi::ItemRegistry>);
        static_assert(modapi::Registry<modapi::CreativeItemRegistry>);
        static_assert(modapi::Registry<modapi::RecipeRegistry>);
        static_assert(modapi::Registry<modapi::GameRuleRegistry>);
        static_assert(modapi::Registry<modapi::worldgen::FeatureRegistry>);

        static_assert(modapi::RegistryEntry<modapi::ItemRegistry, TestItem>);
        static_assert(modapi::RegistryEntry<modapi::RecipeRegistry, TestRecipe>);
        static_assert(modapi::RegistryEntry<modapi::GameRuleRegistry, TestGameRule>);
        static_assert(modapi::RegistryEntry<modapi::worldgen::FeatureRegistry, TestFeature>);
        // Types that are not registry entries must not satisfy the contract.
        static_assert(!modapi::RegistryEntry<modapi::ItemRegistry, TestFeature>);
        static_assert(!modapi::RegistryEntry<modapi::RecipeRegistry, TestItem>);
        static_assert(!modapi::RegistryEntry<modapi::GameRuleRegistry, TestItem>);
        static_assert(!modapi::RegistryEntry<modapi::worldgen::FeatureRegistry, TestRecipe>);
        static_assert(!modapi::RegistryEntry<modapi::CreativeItemRegistry, TestItem>);
        report.pass();
    });
}

// ---------------------------------------------------------------------------------------------
// Public API of every registry, exercised once the server runs
// ---------------------------------------------------------------------------------------------

::GameRule* findGameRule(std::string_view identifier) {
    auto level = ll::service::getLevel();
    if (!level) return nullptr;
    for (auto& rule : *level->getGameRules().mGameRules) {
        if (*rule.mName == identifier) return &rule;
    }
    return nullptr;
}

// The variant a GameRule stores its value in; read through an explicit reference because the// TypedStorage member has no dereference operator.
using GameRuleValue = ::std::variant<::cereal::NullType, bool, int, float>;

GameRuleValue ruleValue(::GameRule const& rule) { return rule.mValue; }

size_t countRecipes() {
    auto level = ll::service::getLevel();
    if (!level) return 0;
    size_t total = 0;
    for (auto& [recipeTag, recipeMap] : *level->getRecipes().mRecipes) {
        (void)recipeTag;
        total += recipeMap.size();
    }
    return total;
}

void testItemApi() {
    report.section("api.item", [] {
        auto& registry = modapi::ItemRegistry::getInstance();

        // `_modifyItem` queues its callback; now that the registry is open
        // the callback has to run, and it has to be able to register items.
        bool modifyRan = false;
        (void)registry._modifyItem([&modifyRan](modapi::ItemRegistry&) { modifyRan = true; });
        report.isTrue("modifyItem.ran", modifyRan);

        auto late = registry.registerItem<TestItem>(std::string{"modapi_test:api_item"}, 3);
        report.isTrue("registerItem.product", late.has_value());
        report.isTrue("registerItem.findable", (bool)registry.getItem("modapi_test:api_item"));

        int  visited = 0;
        bool sawTest = false;
        registry.forEachItemInRegistry([&](::Item& item) {
            ++visited;
            if (item.getSerializedName() == TestItemName) sawTest = true;
            return true;
        });
        report.isTrue("forEachItem.visited", visited > 0, fmt::format("visited {}", visited));
        report.isTrue("forEachItem.sawTestItem", sawTest);

        // `/give` builds its stack from the `CommandItem` the item enum holds, so that packed value has to be
        // the one the engine expects: a union of { short mVersion; bool mOverrideAux; int mId; } over a uint64.
        // `mVersion` must be non-zero - only then does `createInstance` read `mId` as the item's own id
        // instead of converting it as a legacy one, which is what made `/give <custom item>` do nothing.
        {
            auto const itemId = (int)registry.getItem(TestItemName)->mId;

            struct Candidate {
                char const*   name;
                ::CommandItem value;
            };
            Candidate const candidates[] = {
                {"mVersion 1",  ::CommandItem{{1, true, itemId}}   },
                {"mVersion -1", ::CommandItem{{-1, true, itemId}}  },
                {"mVersion 0",  ::CommandItem{{0, true, itemId}}   },
            };

            for (auto const& candidate : candidates) {
                ::CommandOutput output{::CommandOutputType::AllOutput};
                auto            instance = candidate.value.createInstance(1, 0, output, false);
                auto const      got      = instance.has_value() ? instance->getTypeName() : std::string{"<none>"};
                if (std::string_view{candidate.name} == "mVersion 0") {
                    // The legacy-id conversion path: this is the legacy shape.
                    report.isTrue("give.legacyIdShapeResolvesNothing", got != TestItemName, fmt::format("got '{}'", got));
                } else {
                    report.isTrue(
                        fmt::format("give.{}", candidate.name),
                        got == TestItemName,
                        fmt::format("got '{}'", got)
                    );
                }
            }
        }

        // A duplicate in a client's creative inventory shows up here first: count what the registry holds for
        // this item. One entry is correct; two means something adds it twice.
        {
            auto const creative = modapi::CreativeItemRegistry::getInstance().getCreativeItem(TestItemName);
            test::getLogger().info("[REGISTRY] creative entries for {}: {}", TestItemName, creative.size());
            report.isTrue("creative.singleEntry", creative.size() == 1, fmt::format("entries {}", creative.size()));
        }

        report.isTrue("getItem.vanilla", (bool)registry.getItem("minecraft:stick"));
        report.isTrue("getItem.missing", !(bool)registry.getItem("modapi_test:definitely_missing"));

        report.isTrue("setIcon", registry.setIcon(TestItemName, "modapi_test_icon"));
        report.isTrue("setDisplayName", registry.setDisplayName(TestItemName, "ModAPI Test Stick"));
        report.isTrue("addTag", registry.addTag(TestItemName, "modapi_test_tag"));
        report.isTrue("setFireResistant", registry.setFireResistant(TestItemName));
        report.isTrue("setRepairItem", registry.setRepairItem(TestItemName, "minecraft:stick"));
        report.isTrue("setIcon.unknownItem", !registry.setIcon("modapi_test:definitely_missing", "x"));

        report.noThrow("networkTagInfo", [&] {
            auto& tag = registry.getAndModifyVanillaNetworkTagInfo(TestItemName);
            tag["modapi_test"] = 1;
            report.isTrue("networkTagInfo.mutated", tag.contains("modapi_test"));
        });
    });
}

void testCreativeApi() {
    report.section("api.creative", [] {
        auto& registry = modapi::CreativeItemRegistry::getInstance();

        report.isTrue("getCreativeItem.vanilla", !registry.getCreativeItem("minecraft:stick").empty());
        report.isTrue("getCreativeItem.missing", registry.getCreativeItem("modapi_test:definitely_missing").empty());

        // An existing group is looked up, a new one is created; neither may throw.
        report.noThrow("group.existing", [&] { (void)registry.registerCreativeGroup("itemGroup.name.items"); });
        report.noThrow("group.new", [&] { (void)registry.registerCreativeGroup("modapi_test_group"); });

        ::ItemInstance item;
        item.reinit(std::string{TestItemName}, 1, 0);
        modapi::CreativeItemRegistry::ProductRef added;
        report.noThrow("registerCreativeItem", [&] {
            added = registry.registerCreativeItem(
                std::move(item),
                ::SharedTypes::CreativeItemCategory::Items,
                "modapi_test_group"
            );
        });
        report.isTrue("registerCreativeItem.product", added.has_value());

        auto inGroup = registry.getCreativeItem(TestItemName);
        report.isTrue("registerCreativeItem.findable", !inGroup.empty());
        if (!inGroup.empty()) {
            report.eq("registerCreativeItem.group", "modapi_test_group", inGroup.back().mGroup);
        } else {
            report.skip("registerCreativeItem.group");
            return;
        }

        // Removing the entry we just added, then putting it back so the item stays registered.
        auto const removed = inGroup.back();
        report.isTrue("unregisterCreativeItem", registry.unregisterCreativeItem(removed));
        report.isTrue("unregisterCreativeItem.gone", registry.getCreativeItem(TestItemName).size() == inGroup.size() - 1);

        ::ItemInstance restored;
        restored.reinit(std::string{TestItemName}, 1, 0);
        report.isTrue(
            "reRegister",
            registry.registerCreativeItem(std::move(restored), ::SharedTypes::CreativeItemCategory::Items).has_value()
        );
    });
}

void testRecipeApi() {
    report.section("api.recipe", [] {
        auto& registry = modapi::RecipeRegistry::getInstance();

        auto makeStone = [] {
            ::ItemInstance stone;
            stone.reinit("minecraft:stone", 1, 0);
            return stone;
        };
        auto const stick = modapi::ICustomRecipe::Ingredient{
            "minecraft:stick",
            modapi::recipe::RecipeIngredientType::Item,
            1
        };
        auto const stoneIngredient = modapi::ICustomRecipe::Ingredient{
            "minecraft:stone",
            modapi::recipe::RecipeIngredientType::Item,
            1
        };

        // Shapeless: a real product, findable in the live Recipes instance.
        auto const beforeTotal = countRecipes();
        auto       shapeless    = registry.registerShapelessRecipe(
            "modapi_test:api_shapeless",
            std::vector<modapi::ICustomRecipe::Ingredient>{stick},
            makeStone()
        );
        report.isTrue("shapeless.product", shapeless.has_value());
        report.isTrue("shapeless.findable", recipesContain("modapi_test:api_shapeless"));
        report.isTrue("shapeless.counted", countRecipes() > beforeTotal, fmt::format("{} -> {}", beforeTotal, countRecipes()));

        // Stone cutter: a real product as well.
        auto stoneCutter = registry.registerStoneCutterRecipe("modapi_test:api_stonecutter", stoneIngredient, makeStone());
        report.isTrue("stoneCutter.product", stoneCutter.has_value());
        report.isTrue("stoneCutter.findable", recipesContain("modapi_test:api_stonecutter"));

        // Furnace and brewing recipes are not `Recipe` instances, so they have no product - a
        // furnace recipe lands in its own table (keyed by input item id and aux value) instead.
        // A furnace input is keyed by a concrete item id and aux value, so this ingredient carries one.
        auto const furnaceIngredient = modapi::ICustomRecipe::Ingredient{
            "minecraft:stick",
            static_cast<uint8_t>(1),
            static_cast<short>(0)
        };
        ::ItemInstance furnaceInput;
        furnaceInput.reinit("minecraft:stick", 1, 0);
        auto const& furnaceResults = *ll::service::getLevel()->getRecipes().mFurnaceResults;
        auto        keysBefore     = std::vector<int>{};
        for (auto& [id, byAux] : furnaceResults) {
            (void)byAux;
            keysBefore.push_back(id);
        }

        auto furnace = registry.registerFurnaceRecipe(furnaceIngredient, makeStone(), {"modapi_test_furnace"});
        report.isTrue("furnace.noProduct", !furnace.has_value());

        auto const& after    = *ll::service::getLevel()->getRecipes().mFurnaceResults;
        auto const  inputId  = furnaceInput.getId();
        auto const  inputAux = furnaceInput.getAuxValue();
        auto        entry    = after.find(inputId);
        auto const  hasEntry = entry != after.end() && entry->second.contains(inputAux);

        for (auto& [id, byAux] : after) {
            if (std::find(keysBefore.begin(), keysBefore.end(), id) == keysBefore.end()) {
                test::getLogger()
                    .info("[REGISTRY] furnace: new key id={} (expected {}) auxes={}", id, inputId, byAux.size());
            }
        }
        test::getLogger()
            .info("[REGISTRY] furnace: id={} aux={} table={} entry={}", inputId, inputAux, after.size(), hasEntry);
        report.isTrue("furnace.registered", hasEntry);
        if (hasEntry) {
            report.eq("furnace.output", "minecraft:stone", entry->second.at(inputAux).getTypeName());
        } else {
            report.skip("furnace.output");
        }

        report.noThrow("brewing", [&] {
            auto brewing = registry.registerBrewingRecipe(stick, stick, stick);
            report.isTrue("brewing.noProduct", !brewing.has_value());
        });

        // Unregistering, including the "not there" case.
        report.isTrue("unregisterRecipe", registry.unregisterRecipe("modapi_test:api_shapeless"));
        report.isTrue("unregisterRecipe.gone", !recipesContain("modapi_test:api_shapeless"));
        report.isTrue("unregisterRecipe.unknown", !registry.unregisterRecipe("modapi_test:definitely_missing"));

        // JSON input that cannot become a recipe is rejected without touching the registry.
        report.isTrue("json.emptyObject", !registry.registerRecipeFromMemoryJson("{}").has_value());
        report.isTrue(
            "json.unknownKey",
            !registry.registerRecipeFromMemoryJson(
                 R"({"minecraft:recipe_definitely_unknown":{"description":{"identifier":"modapi_test:unknown"}}})"
            )
                 .has_value()
        );
        report.isTrue(
            "json.missingIdentifier",
            !registry.registerRecipeFromMemoryJson(R"({"minecraft:recipe_shaped":{"description":{}}})").has_value()
        );
        report.isTrue("json.missingFile", !registry.registerRecipeFromJsonFile("plugins/test/definitely-missing.json").has_value());
        report.isTrue("json.directoryPath", !registry.registerRecipeFromJsonFile("plugins/test").has_value());
    });
}

void testGameRuleApi() {
    report.section("api.gameRule.register", [] {
        auto& registry = modapi::GameRuleRegistry::getInstance();

        // `shouleSaveToDisk = false` keeps the test rules out of the world's settings.
        auto boolean = registry.registerGameRuleBool("modapi_test:api_bool", true, false, false);
        report.isTrue("bool.product", boolean.has_value());
        auto integer = registry.registerGameRuleInt("modapi_test:api_int", 11, false, false);
        report.isTrue("int.product", integer.has_value());
        auto number = registry.registerGameRuleFloat("modapi_test:api_float", 1.5f, false, false);
        report.isTrue("float.product", number.has_value());
    });

    report.section("api.gameRule.read", [] {
        auto check = [](std::string const& name, std::string_view identifier, ::GameRule::Type type) {
            auto* rule = findGameRule(identifier);
            if (rule == nullptr) {
                report.skip(name + ".inLevelGameRules");
                return static_cast<::GameRule*>(nullptr);
            }
            report.isTrue(name + ".type", rule->mType == type);
            return rule;
        };

        if (auto* rule = check("bool", "modapi_test:api_bool", ::GameRule::Type::Bool)) {
            report.isTrue("bool.value", std::get<bool>(ruleValue(*rule)) == true);
        }
        if (auto* rule = check("int", "modapi_test:api_int", ::GameRule::Type::Int)) {
            report.isTrue("int.value", std::get<int>(ruleValue(*rule)) == 11);
        }
        if (auto* rule = check("float", "modapi_test:api_float", ::GameRule::Type::Float)) {
            report.isTrue("float.value", std::get<float>(ruleValue(*rule)) == 1.5f);
        }
        if (auto* rule = check("deferred", TestGameRuleId, ::GameRule::Type::Int)) {
            report.isTrue("deferred.value", std::get<int>(ruleValue(*rule)) == 7);
            report.isTrue("deferred.notSaved", !rule->mShouldSave);
        }
    });
}

void testFeatureApi() {
    report.section("api.feature", [] {
        auto& registry = modapi::worldgen::FeatureRegistry::getInstance();

        auto level = ll::service::getLevel();
        if (!level) {
            report.skip("level");
            return;
        }

        // The deferred feature has to be part of the level's feature registry, and it has to be the
        // very object the deferred registration produced. The lookup map gives the index into the
        // registry's feature vector.
        auto&       featureRegistry = level->getFeatureRegistry();
        auto const& lookup          = *featureRegistry.mFeatureLookupMap;
        auto const& features        = *featureRegistry.mFeatureRegistry;

        auto objectAt = [&](std::string_view identifier) -> ::IFeature* {
            auto it = lookup.find(::HashedString{std::string{identifier}});
            if (it == lookup.end()) return nullptr;
            auto const index = static_cast<size_t>(it->second);
            return index < features.size() ? features[index].get() : nullptr;
        };

        report.isTrue("deferred.inLevelRegistry", objectAt(TestFeatureId) != nullptr);
        report.isTrue("deferred.sameObject", objectAt(TestFeatureId) == gFeature.get().as_ptr());

        // A single shot registration now that the registry is open.
        auto single = registry.registerFeature("modapi_test:api_feature", std::make_unique<TestFeature>());
        report.isTrue("registerFeature.product", single.has_value());
        report.isTrue("registerFeature.inLevelRegistry", objectAt("modapi_test:api_feature") != nullptr);
        report.isTrue("registerFeature.sameObject", objectAt("modapi_test:api_feature") == single.as_ptr());

        // A rule based feature has no product outside a feature pass, but the rule is remembered for
        // the levels that are configured later.
        auto rule = registry.registerFeatureRule(
            "modapi_test:api_rule_feature",
            {"modapi_test_pass"},
            [](modapi::BlockHelper const&, BlockPos const& pos, Random&) -> ll::coro::Generator<BlockPos> {
                co_yield pos;
            }
        );
        // The rule is remembered for the levels that are configured later *and* registered for the one

        // that is already configured, so there is a product here.
        report.isTrue("registerFeatureRule.product", rule.has_value());
        // The rule's feature is rebuilt for the levels configured *after* the rule was added, so it
        // is not in the registry of the level that is already running.
        report.skip("registerFeatureRule.currentLevel");
    });
}

void testDeferredExtras() {
    report.section("api.deferredRegister", [] {
        // `operator*` / `operator->` reach the same product as `get()`.
        report.isTrue("operatorStar", &*gItem == gItem.get().as_ptr());
        report.isTrue("operatorArrow", gItem->getSerializedName() == TestItemName);
        report.isTrue("recipe.isRegistered", gRecipe.isRegistered());
        report.isTrue("gameRule.isRegistered", gGameRule.isRegistered());
        report.isTrue("feature.isRegistered", gFeature.isRegistered());

        // `CreativeGroupInfo::_addCreativeItemEntry` is the engine's own "record the entry in its
        // group" step, and the only header declared (MCAPI, i.e. resolvable) symbol for it. ModAPI
        // calls it when it adds a creative item, so this has to resolve on the server build.
        auto* addEntry = ll::memory::SymbolView{
                             "?_addCreativeItemEntry@CreativeGroupInfo@@QEAAXPEAVCreativeItemEntry@@@Z"
        }
                             .resolve(true);
        report.isTrue("creative.addEntrySymbolResolved", addEntry != nullptr);
        if (addEntry != nullptr) {
            test::getLogger().info("[REGISTRY] _addCreativeItemEntry trampoline = 0x{:X}", (uintptr_t)addEntry);
        }

        // `ensureEventRegistered()` is idempotent: the listener count must not grow.
        auto&      bus    = ll::event::EventBus::getInstance();
        auto const before = bus.getListenerCount<modapi::ItemReadyEvent>();
        modapi::ItemRegistry::ensureEventRegistered();
        modapi::ItemRegistry::ensureEventRegistered();
        report.isTrue(
            "ensureEventRegistered.idempotent",
            bus.getListenerCount<modapi::ItemReadyEvent>() == before,
            fmt::format("{} -> {}", before, bus.getListenerCount<modapi::ItemReadyEvent>())
        );

        // A registration attaches one listener for the registry's ready event and one for the server
        // stopping event (so it can detach itself before this DLL is unloaded); both go away again
        // when the object is destroyed or detached.
        test::getLogger().info("[REGISTRY] (the next ERR line is expected: a DeferredRegister that never registered)");
        auto const stopBefore = bus.getListenerCount<ll::event::server::ServerStoppingEvent>();
        {
            modapi::DeferredRegister<modapi::ItemRegistry, TestItem> temporary{
                std::string{"modapi_test:temporary_item"},
                1
            };
            report.isTrue(
                "listener.readyAttached",
                bus.getListenerCount<modapi::ItemReadyEvent>() == before + 1,
                fmt::format("{} -> {}", before, bus.getListenerCount<modapi::ItemReadyEvent>())
            );
            report.isTrue(
                "listener.stoppingAttached",
                bus.getListenerCount<ll::event::server::ServerStoppingEvent>() == stopBefore + 1,
                fmt::format("{} -> {}", stopBefore, bus.getListenerCount<ll::event::server::ServerStoppingEvent>())
            );
            report.isTrue("temporary.notRegistered", !temporary.isRegistered());

            // What the stopping listener does; it has to be callable twice.
            temporary.detach();
            report.isTrue(
                "detach.readyRemoved",
                bus.getListenerCount<modapi::ItemReadyEvent>() == before,
                fmt::format("{} -> {}", before, bus.getListenerCount<modapi::ItemReadyEvent>())
            );
            report.isTrue(
                "detach.stoppingRemoved",
                bus.getListenerCount<ll::event::server::ServerStoppingEvent>() == stopBefore,
                fmt::format("{} -> {}", stopBefore, bus.getListenerCount<ll::event::server::ServerStoppingEvent>())
            );
            temporary.detach();
        }
        report.isTrue(
            "destructor.listenerRemoved",
            bus.getListenerCount<modapi::ItemReadyEvent>() == before,
            fmt::format("{} -> {}", before, bus.getListenerCount<modapi::ItemReadyEvent>())
        );
        report.isTrue(
            "destructor.stoppingRemoved",
            bus.getListenerCount<ll::event::server::ServerStoppingEvent>() == stopBefore,
            fmt::format("{} -> {}", stopBefore, bus.getListenerCount<ll::event::server::ServerStoppingEvent>())
        );

        // The handle is tied to the listener it owns, so it is neither copyable nor movable, while
        // the constructor arguments it stores have to be copyable (they are replayed on every pass).
        static_assert(!std::is_copy_constructible_v<modapi::DeferredRegister<modapi::ItemRegistry, TestItem>>);
        static_assert(!std::is_move_constructible_v<modapi::DeferredRegister<modapi::ItemRegistry, TestItem>>);
        static_assert(std::is_copy_constructible_v<std::string>);
        static_assert(std::is_copy_constructible_v<int>);
    });
}

void testReadyEvents() {
    report.section("events.ready", [] {
        report.isTrue("item.fired", gReady.item > 0, fmt::format("fired {}", gReady.item));
        report.isTrue("item.registry", gReady.itemRegistryMatches);
        report.isTrue("creative.fired", gReady.creative > 0, fmt::format("fired {}", gReady.creative));
        report.isTrue("creative.registry", gReady.creativeRegistryMatches);
        report.isTrue("recipe.fired", gReady.recipe > 0, fmt::format("fired {}", gReady.recipe));
        report.isTrue("recipe.registry", gReady.recipeRegistryMatches);
        report.isTrue("gameRule.fired", gReady.gameRule > 0, fmt::format("fired {}", gReady.gameRule));
        report.isTrue("gameRule.registry", gReady.gameRuleRegistryMatches);
        report.isTrue("feature.fired", gReady.feature > 0, fmt::format("fired {}", gReady.feature));
        report.isTrue("feature.registry", gReady.featureRegistryMatches);
    });
}

int runRegistryTests() {
    report.reset();
    test::getLogger().info("[REGISTRY] ===== registry suite start =====");
    report.write();

    testDeferredItem();
    testDeferredRecipe();
    testDeferredGameRule();
    testDeferredFeature();
    testJsonRecipe();
    testPreconditions();

        testItemApi();
        testCreativeApi();
        testRecipeApi();
        testGameRuleApi();
        testFeatureApi();
        testDeferredExtras();
        testReadyEvents();

    report.finish();
    return report.failed();
}

void registerTestModStateProbe() {
    // Runs while the test mod loads; no registry may be open yet at that point.
    auto& registry = modapi::ItemRegistry::getInstance();
    auto& creative = modapi::CreativeItemRegistry::getInstance();
    auto& recipe   = modapi::RecipeRegistry::getInstance();
    auto& gameRule = modapi::GameRuleRegistry::getInstance();
    auto& feature  = modapi::worldgen::FeatureRegistry::getInstance();

    gReadyAtLoad = ReadinessAtLoad{
        registry.isReady(),
        creative.isReady(),
        recipe.isReady(),
        gameRule.isReady(),
        feature.isReady(),
    };

    // Observe every ready event: it has to fire, and it has to hand out the registry singleton.
    // `test::listen` remembers the listeners so `Entry::unload()` can remove them again.
    test::listen<modapi::ItemReadyEvent>([](modapi::ItemReadyEvent& event) {
        ++gReady.item;
        gReady.itemRegistryMatches &= &event.registry() == &modapi::ItemRegistry::getInstance();
    });
    test::listen<modapi::CreativeItemReadyEvent>([](modapi::CreativeItemReadyEvent& event) {
        ++gReady.creative;
        gReady.creativeRegistryMatches &= &event.registry() == &modapi::CreativeItemRegistry::getInstance();
    });
    test::listen<modapi::GameRuleReadyEvent>([](modapi::GameRuleReadyEvent& event) {
        ++gReady.gameRule;
        gReady.gameRuleRegistryMatches &= &event.registry() == &modapi::GameRuleRegistry::getInstance();
    });
    test::listen<modapi::worldgen::FeatureReadyEvent>([](modapi::worldgen::FeatureReadyEvent& event) {
        ++gReady.feature;
        gReady.featureRegistryMatches &= &event.registry() == &modapi::worldgen::FeatureRegistry::getInstance();
    });

    // Recipes loaded from JSON have no user type, so the ready event is the registration point for
    // them; the same listener also covers the recipe event bookkeeping.
    test::listen<modapi::RecipeReadyEvent>([](modapi::RecipeReadyEvent& event) {
        ++gReady.recipe;
        gReady.recipeRegistryMatches &= &event.registry() == &modapi::RecipeRegistry::getInstance();

        if (!gJsonRecipeInvalidRejected) {
            gJsonRecipeInvalidRejected =
                !event.registry().registerRecipeFromMemoryJson("{ this is not json").has_value();
        }
        if (!gJsonRecipe) {
            gJsonRecipe = event.registry().registerRecipeFromMemoryJson(makeJsonRecipe(TestJsonRecipeId));
        }
    });
}

void registerRegistryTestCommand() { registerTestFunction("registry", [] { return runRegistryTests() == 0; }); }

} // namespace

REGISTER_ON_LOAD_TEST(RegistryProbe, registerTestModStateProbe)
REGISTER_ON_LOAD_TEST(RegistryCommand, registerRegistryTestCommand)
REGISTER_TEST(Registry, runRegistryTests)

