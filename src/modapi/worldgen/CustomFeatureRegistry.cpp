#include "modapi/worldgen/CustomFeatureRegistry.h"
#include "modapi/worldgen/LocalData.h"
#include <ll/api/base/Containers.h>
#include <ll/api/memory/Hook.h>
#include <mc/deps/core/string/HashedString.h>
#include <mc/resources/MinEngineVersion.h>
#include <mc/server/PropertiesSettings.h>
#include <mc/world/level/IWorldRegistriesProvider.h>
#include <mc/world/level/levelgen/feature/AutomaticFeatureRules.h>
#include <mc/world/level/levelgen/feature/gamerefs_feature/GameRefsFeature.h>
#include <mc/world/level/levelgen/feature/registry/FeatureRegistry.h>
#include <mc/world/level/levelgen/feature/registry/VanillaFeatures.h>
#include <utility>

namespace modapi::inline worldgen {
struct GlobalData {
    ll::DenseMap<std::string, std::string>               rules;
    ll::DenseMap<std::string, std::unique_ptr<IFeature>> features;
    FeatureRegistry*                                     registry;
    static GlobalData&                                   getInstance() {
        static auto instance = std::make_unique<GlobalData>();
        return *instance;
    }
};
std::string
generateFeatureRule(std::string_view identifier, std::string_view placesFeature, std::string_view placementPass) {
    return std::format(
        R"({{"format_version":"1.21.0","minecraft:feature_rules":{{"description":{{"identifier":"{}","places_feature":"{}"}},"conditions":{{"placement_pass":"{}"}},"distribution":{{"iterations":1,"x":0,"y":0,"z":0}}}}}})",
        identifier,
        placesFeature,
        placementPass
    );
}
void insertRule(std::string_view identifier, std::string_view placesFeature, std::string_view placementPass) {
    GlobalData::getInstance().rules[identifier] = generateFeatureRule(identifier, placesFeature, placementPass);
}
LL_TYPE_INSTANCE_HOOK(
    AutomaticFeatureRule_ParseAndInsertUnsortedHook,
    HookPriority::Normal,
    AutomaticFeatureRules,
    &AutomaticFeatureRules::_parseAndInsertUnsorted,
    Puv::LoadResult<::SharedTypes::v1_21_20::AutomaticFeatureRulesData>,
    ::std::string const&        filename,
    ::std::string&&             data,
    ::MinEngineVersion const&   minEngineVersion,
    ::IWorldRegistriesProvider& worldRegistries,
    ::std::unordered_map<
        ::std::string,
        ::std::unordered_map<::HashedString, ::AutomaticFeatureRules::AutomaticFeatureRule>>& bucketedFeatures,
    bool                                                                                      isBasePack
) {
    unhook();
    for (auto& [identifier, data_] : GlobalData::getInstance().rules) {
        origin(
            identifier.substr(identifier.find(':') + 1),
            std::move(data_),
            minEngineVersion,
            worldRegistries,
            bucketedFeatures,
            isBasePack
        );
    }
    return origin(
        filename,
        std::forward<std::string>(data),
        minEngineVersion,
        worldRegistries,
        bucketedFeatures,
        isBasePack
    );
}
LL_TYPE_INSTANCE_HOOK(
    ClientGenerationHook,
    HookPriority::Normal,
    PropertiesSettings,
    &PropertiesSettings::$ctor,
    void*,
    std::string const& filename
) {
    auto res                                                                 = origin(filename);
    reinterpret_cast<PropertiesSettings*>(res)->mClientSideGenerationEnabled = false;
    return res;
}
LL_TYPE_INSTANCE_HOOK(FeatureRegistryCtorHook, HookPriority::Normal, FeatureRegistry, &FeatureRegistry::$ctor, void*) {
    auto res                           = origin();
    GlobalData::getInstance().registry = (decltype(GlobalData::getInstance().registry))res;
    return res;
}
struct RuleFeature : public IFeature {
    CustomFeatureRegistry::CustomFeatureRule mRule;
    std::string                              mName;
    RuleFeature(CustomFeatureRegistry::CustomFeatureRule&& rule, std::string_view name)
    : mRule(std::forward<decltype(mRule)>(rule)),
      mName(name) {}
    ::std::optional<::BlockPos> place(::IFeature::PlacementContext const& context) const override {
        std::optional<BlockPos>      res = std::nullopt;
        auto                         pos = context.mPos;
        std::shared_ptr<BlockHelper> helper{};
        static auto                  feature = [](std::string_view name) {
            auto idx = GlobalData::getInstance().registry->mFeatureLookupMap->at({name});
            return GlobalData::getInstance().registry->mFeatureRegistry->at(idx).get();
        }(mName);
        if (LocalData::getInstance().mBlockSource) {
            helper = std::make_shared<BlockHelper>(LocalData::getInstance().mBlockSource);
        } else {
            helper = std::make_shared<BlockHelper>(LocalData::getInstance().mLevelChunk);
        }
        for (auto placePos : mRule(*helper, pos, *LocalData::getInstance().mRandom)) {
            const_cast<::IFeature::PlacementContext&>(context).mPos = placePos;
            res                                                     = feature->place(context);
        }
        return res;
    }
    bool isValidPlacement(::std::string const& pass) override {
        static auto feature = [](std::string_view name) {
            auto idx = GlobalData::getInstance().registry->mFeatureLookupMap->at({name});
            return GlobalData::getInstance().registry->mFeatureRegistry->at(idx).get();
        }(mName);
        return feature->isValidPlacement(pass);
    }
    void upgradeFormat(::SemVersion const& ver) override {
        static auto feature = [](std::string_view name) {
            auto idx = GlobalData::getInstance().registry->mFeatureLookupMap->at({name});
            return GlobalData::getInstance().registry->mFeatureRegistry->at(idx).get();
        }(mName);
        return feature->upgradeFormat(ver);
    }
    ~RuleFeature() override = default;
};
LL_STATIC_HOOK(
    VanillaFeaturesRegisterFeaturesHook,
    HookPriority::Normal,
    &VanillaFeatures::registerFeatures,
    void,
    class FeatureRegistry&       registry,
    class BaseGameVersion const& baseGameVersion,
    class Experiments const&     experiments
) {
    for (auto& [identifier, feature] : GlobalData::getInstance().features) {
        registry._registerFeature(identifier, std::move(feature));
    }
    origin(registry, baseGameVersion, experiments);
}

struct CustomFeatureRegistry::Impl {
    ll::memory::HookRegistrar<
        AutomaticFeatureRule_ParseAndInsertUnsortedHook,
        ClientGenerationHook,
        FeatureRegistryCtorHook,
        VanillaFeaturesRegisterFeaturesHook>
        mHooks;
};
CustomFeatureRegistry::CustomFeatureRegistry() { pImpl = std::make_unique<Impl>(); }
CustomFeatureRegistry&
CustomFeatureRegistry::registerFeature(std::string_view identifier, std::unique_ptr<IFeature> feature) {
    GlobalData::getInstance().features.emplace(identifier, std::move(feature));
    return *this;
}
CustomFeatureRegistry& CustomFeatureRegistry::registerFeatureRule(
    std::string_view                identifier,
    std::vector<std::string> const& passes,
    CustomFeatureRule               rule
) {
    static uint64 idx{0};
    auto          ruleId = std::format("gmlib:interal_{}", idx++);
    GlobalData::getInstance().features.emplace(
        ruleId,
        std::unique_ptr<IFeature>(new RuleFeature{std::move(rule), identifier})
    );
    for (auto& pass : passes) {
        auto passId = std::format("{}_{}", ruleId, pass);
        GlobalData::getInstance().rules.emplace(passId, generateFeatureRule(passId, ruleId, pass));
    }
    return *this;
}
CustomFeatureRegistry& CustomFeatureRegistry::getInstance() {
    static auto instance = std::make_unique<CustomFeatureRegistry>();
    return *instance;
}
} // namespace modapi::inline worldgen