#include "modapi/worldgen/FeatureRegistry.h"
#include "mc/deps/puv/LoadResult.h"
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

// The document the engine parses for one pass. Its shape follows the files the server ships under
// `definitions/feature_rules/`: the same `format_version`, and a distribution whose `x`/`y`/`z` are uniform extents
// rather than bare numbers.
std::string
generateFeatureRule(std::string_view identifier, std::string_view placesFeature, std::string_view placementPass) {
    return std::format(
        R"({{"format_version":"1.13.0","minecraft:feature_rules":{{"description":{{"identifier":"{}","places_feature":"{}"}},"conditions":{{"placement_pass":"{}"}},"distribution":{{"iterations":1,"coordinate_eval_order":"zxy","x":{{"distribution":"uniform","extent":[0,16]}},"y":{{"distribution":"uniform","extent":[0,16]}},"z":{{"distribution":"uniform","extent":[0,16]}}}}}}}})",
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
        // NOTE: this parser is being called with a document that did not come from a pack, and every attempt to do so
        // aborts the engine inside `origin` (0xC0000005, and 0xC000001D with a name that outlives the call). A rule
        // that is meant to reach a live level therefore has to be delivered as a file in a behaviour pack instead.
        std::string ruleFileName = identifier.substr(identifier.find(':') + 1);
        origin(ruleFileName, std::move(data_), minEngineVersion, worldRegistries, bucketedFeatures, isBasePack);
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

    // The feature this rule places, looked up in the registry of the level that is running. It is looked up per call,
    // not cached: the registry is filled for every level, and the wrapper is built before the features of a level are
    // in it. A missing name is a miss, not a throw - the id is generated by ModAPI and the level may not know it yet.
    ::IFeature* placedFeature() const {
        auto& data = GlobalData::getInstance();
        if (data.registry == nullptr) return nullptr;
        auto const& lookup = data.registry->mFeatureLookupMap;
        auto        it     = lookup->find(::HashedString{mName});
        if (it == lookup->end()) return nullptr;
        auto const index = static_cast<size_t>(it->second);
        if (index >= data.registry->mFeatureRegistry->size()) return nullptr;
        return data.registry->mFeatureRegistry->at(index).get();
    }

    ::std::optional<::BlockPos> place(::IFeature::PlacementContext const& context) const override {
        auto* feature = placedFeature();
        if (feature == nullptr) return std::nullopt;

        // Everything below comes from the generation hooks. During a pass that does not fill them there is nothing to
        // place into, and dereferencing the random source regardless is what aborted the server.
        auto& local = LocalData::getInstance();
        if (local.mRandom == nullptr) return std::nullopt;

        std::shared_ptr<BlockHelper> helper = local.mBlockSource != nullptr
                                                ? std::make_shared<BlockHelper>(local.mBlockSource)
                                                : std::make_shared<BlockHelper>(local.mLevelChunk);

        std::optional<BlockPos> res = std::nullopt;
        for (auto placePos : mRule->place(*helper, context.mPos, *local.mRandom)) {
            const_cast<::IFeature::PlacementContext&>(context).mPos = placePos;
            res                                                     = feature->place(context);
        }
        return res;
    }

    bool isValidPlacement(::std::string const& pass) override {
        auto* feature = placedFeature();
        return feature != nullptr && feature->isValidPlacement(pass);
    }

    void upgradeFormat(::SemVersion const& ver) override {
        if (auto* feature = placedFeature()) feature->upgradeFormat(ver);
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
    std::string const&                  placesFeature,
    std::vector<std::string> const&     passes,
    std::unique_ptr<ICustomFeatureRule> rule
) {
    if (!rule) return {};
    static uint64 idx{0};
    auto          ruleId = std::format("modapi:rule_{}", idx++);
    auto          shared = std::shared_ptr<ICustomFeatureRule>{std::move(rule)};
    auto&         data   = GlobalData::getInstance();

    // The engine drives the rule through this wrapper, which is what `places_feature` names. It places nothing itself:
    // for every position the rule yields it moves the context there and hands it to the feature the rule was registered
    // with, and that feature is the one that writes.
    data.features[ruleId] = [shared, placesFeature]() -> std::unique_ptr<IFeature> {
        return std::make_unique<RuleFeature>(shared, placesFeature);
    };
    // One document per pass, named after the rule the mod registered so a log or a pack dump points back at it.
    for (auto& pass : passes) {
        auto passId = std::format("{}_{}", identifier, pass);
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

Puv::LoadResultAny::~LoadResultAny() = default;