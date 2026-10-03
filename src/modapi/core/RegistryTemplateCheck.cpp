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

[[maybe_unused]] void deferredRegisterTemplateCheck() {
    static DeferredRegister<item::ItemRegistry, CheckItem>             checkItem{"modapi:template_check"};
    static DeferredRegister<recipe::RecipeRegistry, CheckRecipe>       checkRecipe;
    static DeferredRegister<gamerule::GameRuleRegistry, CheckGameRule> checkGameRule;
    // Custom features take their identifier as the first constructor argument.
    static DeferredRegister<worldgen::FeatureRegistry, CheckFeature> checkFeature{"modapi:template_check"};
}

} // namespace

} // namespace modapi::inline core
