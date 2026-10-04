// Compile-time check of the registry templates.
//
// `DeferredRegister<Registry, Entry>` is a header only template, so nothing in ModAPI itself would
// otherwise instantiate it and a broken registry contract would only show up in a consumer's build.
// The function below is never called - it exists purely so the compiler instantiates the deferred
// registration path for every registry that supports it. The sample entries are never registered.

#include "modapi/DeferredRegister.h"
#include "modapi/gamerule/GameRuleRegistry.h"
#include "modapi/item/ItemRegistry.h"
#include "modapi/item/shared_types/ItemInitializer.h"
#include "modapi/item/shared_types/NetworkTagBuilder.h"
#include "modapi/recipe/RecipeRegistry.h"
#include "modapi/worldgen/FeatureRegistry.h"
#include "modapi/worldgen/FeatureRuleRegistry.h"
#include <optional>
#include <string>
#include <vector>

namespace modapi::inline core {

namespace {

class CheckItem : public item::ICustomItem<::Item> {
public:
    explicit CheckItem(std::string const& identifier) : ICustomItem(identifier) {}

    ItemIcon getIcon() const override { return {}; }
};

class CheckRecipe : public recipe::ICustomRecipe {
public:
    std::string getRecipeId() const override { return "modapi:template_check"; }

    std::vector<std::string> getCraftingTags() const override { return {}; }

    void _init() {}
};

class CheckGameRule : public gamerule::ICustomGameRule<bool> {
public:
    std::string getIdentifier() const override { return "modapi:template_check"; }

    bool getDefaultValue() const override { return false; }
};

class CheckFeature : public worldgen::ICustomFeature {
public:
    std::optional<BlockPos> place(worldgen::BlockHelper&, BlockPos const&, Random&) const override {
        return std::nullopt;
    }
};

class CheckRule : public worldgen::ICustomFeatureRule {
public:
    ll::coro::Generator<BlockPos> place(BlockHelper const&, BlockPos const& pos, Random&) override { co_yield pos; }
};

[[maybe_unused]] void deferredRegisterTemplateCheck() {
    static DeferredRegister<item::ItemRegistry, CheckItem>             checkItem{"modapi:template_check"};
    static DeferredRegister<recipe::RecipeRegistry, CheckRecipe>       checkRecipe;
    static DeferredRegister<gamerule::GameRuleRegistry, CheckGameRule> checkGameRule;
    // A feature and a rule carry their identifier in the registration structure.
    static DeferredRegister<worldgen::FeatureRegistry, CheckFeature> checkFeature{
        worldgen::FeatureRegistry::FeatureRegistration<>{.mIdentifier = "modapi:template_check"}
    };
    static DeferredRegister<worldgen::FeatureRuleRegistry, CheckRule> checkRule{
        worldgen::FeatureRuleRegistration<>{.mIdentifier = "modapi:template_check"}
    };
}

} // namespace

} // namespace modapi::inline core
