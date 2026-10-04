#include "modapi/worldgen/FeatureRegistry.h"
#include "modapi/core/RegistryEvent.h"
#include "modapi/worldgen/FeatureRuleRegistry.h"
#include "modapi/worldgen/LocalData.h"
#include <atomic>
#include <ll/api/base/Containers.h>
#include <ll/api/event/EventBus.h>
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
    ll::DenseMap<std::string, std::string> rules;
    // Factories instead of instances: vanilla registers the features of every configured level, so
    // each pass needs its own feature objects.
    ll::DenseMap<std::string, std::function<std::unique_ptr<IFeature>()>> features;
    ::FeatureRegistry*                                                    registry = nullptr;

    static GlobalData& getInstance() {
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
    void*
) {
    auto res                                                                 = origin();
    reinterpret_cast<PropertiesSettings*>(res)->mClientSideGenerationEnabled = false;
    return res;
}
LL_TYPE_INSTANCE_HOOK(
    FeatureRegistryCtorHook,
    HookPriority::Normal,
    ::FeatureRegistry,
    &::FeatureRegistry::$ctor,
    void*,
    ::Bedrock::NonOwnerPointer<::LinkedAssetValidator> validator
) {
    auto res                           = origin(validator);
    GlobalData::getInstance().registry = (decltype(GlobalData::getInstance().registry))res;
    return res;
}

struct RuleFeature : public IFeature {
    std::shared_ptr<ICustomFeatureRule> mRule;
    std::string                         mName;

    RuleFeature(std::shared_ptr<ICustomFeatureRule> rule, std::string_view name)
    : mRule(std::move(rule)),
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
        for (auto placePos : mRule->place(*helper, pos, *LocalData::getInstance().mRandom)) {
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

// `VanillaGameModuleServer::configureLevel` (and the client equivalent) call this for every level
// they configure, so the deferred entries are rebuilt for each registry instance.
LL_STATIC_HOOK(
    VanillaFeaturesRegisterFeaturesHook,
    HookPriority::Normal,
    &VanillaFeatures::registerFeatures,
    void,
    ::FeatureRegistry&           registry,
    class BaseGameVersion const& baseGameVersion,
    class Experiments const&     experiments
) {
    auto& self = FeatureRegistry::getInstance();
    self._bindRegistry(registry);
    self._applyFeatureFactories(registry);
    // A rule refers to the feature it places, and the engine parses the rules out of the level later, so the rule
    // definitions have to be in place before `origin` too.
    FeatureRuleRegistry::getInstance()._bindRegistry(registry);
    origin(registry, baseGameVersion, experiments);
}

struct FeatureRegistry::Impl {
    ll::memory::HookRegistrar<
        AutomaticFeatureRule_ParseAndInsertUnsortedHook,
        ClientGenerationHook,
        FeatureRegistryCtorHook,
        VanillaFeaturesRegisterFeaturesHook>
                       mHooks;
    ::FeatureRegistry* mRegistry = nullptr;
    bool               mReady    = false;
    // Features handed over through the direct API before a pass ran; single shot, they are moved
    // into the first registry that is configured afterwards.
    ll::DenseMap<std::string, std::unique_ptr<IFeature>> mPendingFeatures;
};

FeatureRegistry::FeatureRegistry() { pImpl = std::make_unique<Impl>(); }

FeatureRegistry& FeatureRegistry::getInstance() {
    // Deliberately leaked: the hook registrar this instance owns must not unregister its hooks while
    // the process/DLL is being torn down.
    static auto& instance = *new FeatureRegistry();
    return instance;
}

bool FeatureRegistry::isReady() const noexcept { return pImpl->mReady; }

void FeatureRegistry::ensureEventRegistered() {
    // The vanilla hooks live in the registry's implementation, so the singleton has to exist before
    // the pass that fires them; every consumer goes through this entry point.
    (void)getInstance();
    static std::atomic_bool registered = false;
    core::ensureRegistryEventRegistered<EventType>(registered);
}

void FeatureRegistry::_bindRegistry(::FeatureRegistry& registry) {
    pImpl->mRegistry = &registry;
    pImpl->mReady    = true;
    ll::event::EventBus::getInstance().publish(FeatureReadyEvent{*this});
}

FeatureRegistry::ProductRef
FeatureRegistry::_registerFeature(std::string_view identifier, std::unique_ptr<IFeature> feature) {
    if (pImpl->mRegistry == nullptr || !feature) return {};
    try {
        auto* raw = feature.get();
        pImpl->mRegistry->_registerFeature(std::string{identifier}, std::move(feature));
        return raw;
    } catch (...) {
        return {};
    }
}

FeatureRegistry::ProductRef
FeatureRegistry::registerFeature(std::string_view identifier, std::unique_ptr<IFeature> feature) {
    if (pImpl->mRegistry != nullptr) return _registerFeature(identifier, std::move(feature));
    if (!feature) return {};
    // No pass is running: hand it over to the next one.
    pImpl->mPendingFeatures[std::string{identifier}] = std::move(feature);
    return {};
}

FeatureRuleRegistry& FeatureRuleReadyEvent::registry() const { return mRegistry; }

struct FeatureRuleRegistry::Impl {
    // The registry the features of the current level went into; the rules are parsed out of it.
    ::FeatureRegistry* mRegistry = nullptr;
    bool               mReady    = false;
};

FeatureRuleRegistry::FeatureRuleRegistry() { pImpl = std::make_unique<Impl>(); }

FeatureRuleRegistry& FeatureRuleRegistry::getInstance() {
    static FeatureRuleRegistry instance;
    return instance;
}

bool FeatureRuleRegistry::isReady() const noexcept { return pImpl->mReady; }

void FeatureRuleRegistry::ensureEventRegistered() {
    (void)getInstance();
    static std::atomic_bool registered = false;
    core::ensureRegistryEventRegistered<FeatureRuleReadyEvent>(registered);
}

FeatureRuleRegistry::ProductRef FeatureRuleRegistry::_registerRule(
    std::string const&                  identifier,
    std::vector<std::string> const&     passes,
    std::unique_ptr<ICustomFeatureRule> rule
) {
    if (!rule) return {};
    static uint64 idx{0};
    auto          ruleId           = std::format("modapi:rule_{}", idx++);
    std::string   identifierString = identifier;
    auto          shared           = std::shared_ptr<ICustomFeatureRule>{std::move(rule)};
    auto&         data             = GlobalData::getInstance();

    // The feature is rebuilt for every level, and the engine's rules refer to it by the id generated here.
    data.features[ruleId] = [shared, identifierString]() -> std::unique_ptr<IFeature> {
        return std::make_unique<RuleFeature>(shared, identifierString);
    };
    for (auto& pass : passes) {
        auto passId = std::format("{}_{}", ruleId, pass);
        data.rules.emplace(passId, generateFeatureRule(passId, ruleId, pass));
    }

    if (pImpl->mRegistry != nullptr) {
        if (auto& factory = data.features.at(ruleId); factory) {
            return FeatureRegistry::getInstance()._registerFeature(ruleId, factory());
        }
    }
    return {};
}

void FeatureRuleRegistry::_bindRegistry(::FeatureRegistry& registry) {
    pImpl->mRegistry = &registry;
    pImpl->mReady    = true;
    ll::event::EventBus::getInstance().publish(FeatureRuleReadyEvent{*this});
}

void FeatureRegistry::_applyFeatureFactories(::FeatureRegistry& registry) {
    auto& data = GlobalData::getInstance();

    // Rule based features are rebuilt for every pass.
    for (auto& [identifier, factory] : data.features) {
        if (!factory) continue;
        try {
            if (auto feature = factory()) registry._registerFeature(identifier, std::move(feature));
        } catch (...) {}
    }
    // Features handed over through the direct API before this pass.
    for (auto& [identifier, feature] : pImpl->mPendingFeatures) {
        if (!feature) continue;
        try {
            registry._registerFeature(identifier, std::move(feature));
        } catch (...) {}
    }
    pImpl->mPendingFeatures.clear();
}

FeatureRegistry& FeatureReadyEvent::registry() const { return mRegistry; }

} // namespace modapi::inline worldgen
