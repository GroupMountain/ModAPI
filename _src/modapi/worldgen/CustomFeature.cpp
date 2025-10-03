
#include <modapi/worldgen/FeatureRegistry.h>
#include <modapi/worldgen/ICustomFeature.h>


#include <ll/api/memory/Hook.h>
#include <ll/api/service/Bedrock.h>

#include <mc/deps/core/string/HashedString.h>
#include <mc/deps/game_refs/GameRefs.h>
#include <mc/deps/game_refs/WeakRef.h>
#include <mc/server/PropertiesSettings.h>
#include <mc/world/level/BlockPos.h>
#include <mc/world/level/BlockSource.h>
#include <mc/world/level/Level.h>
#include <mc/world/level/biome/components/BiomeDecorationFeature.h>
#include <mc/world/level/biome/systems/BiomeDecorationSystem.h>
#include <mc/world/level/chunk/LevelChunk.h>
#include <mc/world/level/dimension/Dimension.h>
#include <mc/world/level/levelgen/feature/AutomaticFeatureRules.h>
#include <mc/world/level/levelgen/feature/IFeature.h>
#include <mc/world/level/levelgen/feature/gamerefs_feature/GameRefsFeature.h>
#include <mc/world/level/levelgen/feature/registry/FeatureRegistry.h>
#include <mc/world/level/levelgen/feature/registry/VanillaFeatures.h>
#include <mc/world/level/storage/Experiments.h>


namespace gmlib::mod {

bool registered = false;
using Rule      = std::function<ll::coro::Generator<BlockPos>(const BlockHelper&, const BlockPos&, Random&)>;

std::unordered_map<std::string, std::string> featureRules;

LL_TYPE_INSTANCE_HOOK(
    AutomaticFeatureRule_ParseAndInsertUnsortedHook,
    HookPriority::Normal,
    AutomaticFeatureRules,
    &AutomaticFeatureRules::_parseAndInsertUnsorted,
    void,
    ::std::string const&        filename,
    ::std::string&&             data,
    ::MinEngineVersion const&   minEngineVersion,
    ::IWorldRegistriesProvider& worldRegistries,
    ::std::unordered_map<
        ::std::string,
        ::std::unordered_map<::HashedString, ::AutomaticFeatureRules::AutomaticFeatureRule>>& bucketedFeatures,
    bool                                                                                      isBasePack
) {
    if (registered) {
        return origin(
            filename,
            std::forward<std::string>(data),
            minEngineVersion,
            worldRegistries,
            bucketedFeatures,
            isBasePack
        );
    }
    registered = true;
    for (auto& [identifier, data_] : featureRules) {
        origin(
            identifier.substr(identifier.find(':') + 1),
            std::move(data_),
            minEngineVersion,
            worldRegistries,
            bucketedFeatures,
            isBasePack
        );
    }
    origin(filename, std::forward<std::string>(data), minEngineVersion, worldRegistries, bucketedFeatures, isBasePack);
}

std::string
generateFeatureRule(const std::string& identifier, const std::string& placesFeature, const std::string& placementPass) {
    return std::format(
        R"({{"format_version":"1.21.0","minecraft:feature_rules":{{"description":{{"identifier":"{}","places_feature":"{}"}},"conditions":{{"placement_pass":"{}"}},"distribution":{{"iterations":1,"x":0,"y":0,"z":0}}}}}})",
        identifier,
        placesFeature,
        placementPass
    );
}

void insertRule(const std::string& identifier, const std::string& placesFeature, const std::string& placementPass) {
    featureRules[identifier] = generateFeatureRule(identifier, placesFeature, placementPass);
}


thread_local struct {
    BlockSource* blockSource = nullptr;
    Random*      random      = nullptr;
    LevelChunk*  chunk       = nullptr;
} featureThreadLocalData;


LL_STATIC_HOOK(
    BiomeDecorationSystemDecorateBiomeHook,
    HookPriority::Normal,
    &::BiomeDecorationSystem::decorateBiome,
    bool,
    ::LevelChunk&                               lc,
    ::BlockSource&                              source,
    ::Random&                                   random,
    ::gsl::span<::BiomeDecorationFeature const> featureList,
    ::std::string const&                        pass,
    ::Biome const*                              biome,
    ::IPreliminarySurfaceProvider const&        preliminarySurfaceProvider
) {
    featureThreadLocalData.blockSource = &source;
    featureThreadLocalData.random      = &random;
    featureThreadLocalData.chunk       = nullptr;
    return origin(lc, source, random, featureList, pass, biome, preliminarySurfaceProvider);
}

LL_STATIC_HOOK(
    BiomeDecorationSystemDecorateLargeFeatureHook,
    HookPriority::Normal,
    &::BiomeDecorationSystem::decorateLargeFeature,
    void,
    ::Biome const&       biome,
    ::LevelChunk&        lc,
    ::BlockVolumeTarget& target,
    ::Random&            random,
    ::ChunkPos const&    pos,
    ::std::string const& pass
) {
    featureThreadLocalData.chunk       = &lc;
    featureThreadLocalData.random      = &random;
    featureThreadLocalData.blockSource = nullptr;
    origin(biome, lc, target, random, pos, pass);
}

ICustomFeature::ICustomFeature()  = default;
ICustomFeature::~ICustomFeature() = default;
::std::optional<::BlockPos> ICustomFeature::place(::IFeature::PlacementContext const& context) const {
    if (featureThreadLocalData.blockSource) {
        std::shared_ptr<BlockHelper> helper = std::make_shared<BlockHelper>(featureThreadLocalData.blockSource);
        return this->place(*helper, context.mUnke9f615.as<BlockPos>(), *featureThreadLocalData.random);
    } else {
        std::shared_ptr<BlockHelper> helper = std::make_shared<BlockHelper>(featureThreadLocalData.chunk);
        return this->place(*helper, context.mUnke9f615.as<BlockPos>(), *featureThreadLocalData.random);
    }
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

std::unordered_map<std::string, std::unique_ptr<IFeature>> features;

struct FeatureRule {
    std::string              placeFeature;
    std::vector<std::string> passes;
    Rule                     rule;
};

std::unordered_map<std::string, FeatureRule> rules;

struct RuleFeature : public IFeature {
    Rule                mRule;
    ::WeakRef<IFeature> mFeature;
    RuleFeature(Rule&& rule, WeakRef<IFeature>&& feature)
    : mRule(std::forward<decltype(mRule)>(rule)),
      mFeature(std::forward<decltype(mFeature)>(feature)) {}
    [[nodiscard]] ::std::optional<::BlockPos> place(::IFeature::PlacementContext const& context) const override {
        std::optional<BlockPos> res = std::nullopt;
        auto                    pos = context.mUnke9f615.as<BlockPos>();
        if (featureThreadLocalData.blockSource) {
            std::shared_ptr<BlockHelper> helper = std::make_shared<BlockHelper>(featureThreadLocalData.blockSource);
            for (auto placePos : mRule(*helper, pos, *featureThreadLocalData.random)) {
                const_cast<::IFeature::PlacementContext&>(context).mUnke9f615.as<BlockPos>() = placePos;
                res = mFeature.mRegistry.get()->get().mFeatureRegistry->at(mFeature.mIndex)->place(context);
            }
        } else {
            std::shared_ptr<BlockHelper> helper = std::make_shared<BlockHelper>(featureThreadLocalData.chunk);
            for (auto placePos : mRule(*helper, pos, *featureThreadLocalData.random)) {
                const_cast<::IFeature::PlacementContext&>(context).mUnke9f615.as<BlockPos>() = placePos;
                res = mFeature.mRegistry.get()->get().mFeatureRegistry->at(mFeature.mIndex)->place(context);
            }
        }


        return res;
    }
    bool isValidPlacement(::std::string const& pass) override {
        return mFeature.mRegistry.get()->get().mFeatureRegistry->at(mFeature.mIndex)->isValidPlacement(pass);
    }
    void upgradeFormat(::SemVersion const& ver) override {
        mFeature.mRegistry.get()->get().mFeatureRegistry->at(mFeature.mIndex)->upgradeFormat(ver);
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
    for (auto& [identifier, feature] : features) {
        registry._registerFeature(identifier, std::move(feature));
    }
    origin(registry, baseGameVersion, experiments);
    for (auto& [identifier, rule] : rules) {
        registry._registerFeature(
            std::format("gmlib:internal_{}", std::hash<std::string>{}(identifier)),
            std::unique_ptr<IFeature>(
                new RuleFeature(std::move(rule.rule), registry.lookupOrReserveFeature(rule.placeFeature))
            )
        );
    }
}

bool featuresRegistered = false;

std::unordered_map<std::string, std::string> jsonFeatures;

LL_TYPE_INSTANCE_HOOK(
    FeatureRegistry_SetupFeatureHook,
    HookPriority::Normal,
    FeatureRegistry,
    &FeatureRegistry::_setupFeature,
    bool,
    ::IWorldRegistriesProvider&  worldRegistries,
    bool                         isBasePack,
    ::ResourcePackManager const& rpm,
    ::std::string const&         featureName,
    ::std::string const&         jsonDefinition,
    ::MinEngineVersion const&    minEngineVersion,
    bool                         serializeFeatures
) {
    if (featuresRegistered)
        return origin(
            worldRegistries,
            isBasePack,
            rpm,
            featureName,
            jsonDefinition,
            minEngineVersion,
            serializeFeatures
        );
    featuresRegistered = true;
    for (auto& [name, definition] : jsonFeatures)
        origin(worldRegistries, isBasePack, rpm, name, definition, minEngineVersion, serializeFeatures);
    return origin(worldRegistries, isBasePack, rpm, featureName, jsonDefinition, minEngineVersion, serializeFeatures);
}


void init() {
    static std::unique_ptr<ll::memory::HookRegistrar<
        AutomaticFeatureRule_ParseAndInsertUnsortedHook,
        BiomeDecorationSystemDecorateBiomeHook,
        BiomeDecorationSystemDecorateLargeFeatureHook,
        ClientGenerationHook,
        VanillaFeaturesRegisterFeaturesHook,
        FeatureRegistry_SetupFeatureHook>>
        featureHooksRegistrar = std::make_unique<decltype(featureHooksRegistrar)::element_type>();
}

void registerFeature(const std::string& identifier, std::unique_ptr<IFeature> feature) {
    init();
    features.emplace(identifier, std::move(feature));
}

void registerFeatureRule(
    const std::string&              identifier,
    const std::vector<std::string>& passes,
    const std::string&              placeFeature,
    Rule                            rule
) {
    init();
    for (auto& pass : passes)
        insertRule(
            std::format("{}_{}", identifier, pass),
            std::format("gmlib:internal_{}", std::hash<std::string>{}(identifier)),
            pass
        );
    rules.emplace(identifier, FeatureRule{placeFeature, passes, std::move(rule)});
}
void registerFeature(const nlohmann::json& json) {
    init();
    for (auto& [key, obj] : json.items()) {
        if (key.starts_with("minecraft:")) {
            jsonFeatures[obj["description"]["identifier"]] = json.dump();
            break;
        }
    }
}
void registerFeatureRule(const nlohmann::json& json) {
    init();
    featureRules[json["minecraft:feature_rules"]["description"]["identifier"]] = json.dump();
}
} // namespace gmlib::mod