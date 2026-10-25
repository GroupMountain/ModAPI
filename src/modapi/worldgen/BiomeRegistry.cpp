#include "modapi/worldgen/BiomeRegistry.h"
#include "modapi/core/Gloabl.h"
#include "modapi/core/RegistryEvent.h"
#include "modapi/worldgen/SurfaceBuilderRegistry.h"
#include <array>
#include <atomic>
#include <cstring>
#include <ll/api/base/Containers.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/server/ServerStartedEvent.h>
#include <ll/api/memory/Hook.h>
#include <ll/api/service/Bedrock.h>
#include <mc/server/commands/CommandRegistry.h>
#include <mc/world/level/BlockPalette.h>
#include <mc/world/level/IWorldRegistriesProvider.h>
#include <mc/world/level/Level.h>
#include <mc/world/level/biome/Biome.h>
#include <mc/world/level/biome/BiomeNoiseTarget.h>
#include <mc/world/level/biome/ClimateParameters.h>
#include <mc/world/level/biome/ParameterList.h>
#include <mc/world/level/biome/RTree.h>
#include <mc/world/level/biome/VanillaBiomes.h>
#include <mc/world/level/biome/climate_utils/Parameter.h>
#include <mc/world/level/biome/components/BiomeComponentStorage.h>
#include <mc/world/level/biome/registry/BiomeRegistry.h>
#include <mc/world/level/biome/serialization/BiomeDefinitionChunkGenData.h>
#include <mc/world/level/biome/serialization/BiomeDefinitionData.h>
#include <mc/world/level/biome/serialization/BiomeStringList.h>
#include <mc/world/level/biome/serialization/BiomeSurfaceBuilderData.h>
#include <mc/world/level/biome/serialization/SerializedNoiseBlockSpecifier.h>
#include <mc/world/level/biome/serialization/VanillaBiomeSerialization.h>
#include <mc/world/level/biome/systems/BiomeSurfaceSystem.h>
#include <mc/world/level/biome/systems/biome_surface_system/SurfaceBuilderParamsResult.h>
#include <mc/world/level/block/BlockVolume.h>
#include <mc/world/level/chunk/LevelChunk.h>
#include <mc/world/level/dimension/DimensionHeightRange.h>
#include <mc/world/level/levelgen/feature/registry/FeatureRegistry.h>
#include <mc/world/level/levelgen/v1/SurfaceLevelCache.h>
#include <utility>

namespace modapi::inline worldgen {

// Ids from here up belong to mods; the engine reserves the ones below for vanilla biomes.
constexpr unsigned short kFirstCustomBiomeId = 30000;

namespace {

// `TerrainBuilderComponent::mSurfaceBuilderData` sits right after the component's vtable, and the
// component objects are the ones `Biome::mBiomeComponentStorage` owns. Comparing the address is all
// that is needed to tell whether a chunk's surface parameters belong to a biome.
::BiomeSurfaceBuilderData makeSurface(BiomeProperty const& property) {
    ::BiomeSurfaceBuilderData surface{};
    auto const*               preset = ::std::get_if<BiomeProperty::SurfacePreset>(&property.mSurface);
    switch (preset != nullptr ? *preset : BiomeProperty::SurfacePreset::Overworld) {
    case BiomeProperty::SurfacePreset::Swamp:
        surface.mHasSwampSurface = true;
        break;
    case BiomeProperty::SurfacePreset::FrozenOcean:
        surface.mHasFrozenOceanSurface = true;
        break;
    case BiomeProperty::SurfacePreset::TheEnd:
        surface.mHasTheEndSurface = true;
        break;
    case BiomeProperty::SurfacePreset::Overworld:
    default:
        surface.mHasDefaultOverworldSurface = true;
        break;
    }
    return surface;
}

bool ownsSurfaceData(::Biome const& biome, void const* data) {
    for (auto& entry : biome.mBiomeComponentStorage->mComponentList.get()) {
        if (static_cast<::std::byte const*>(data) == reinterpret_cast<::std::byte const*>(entry.second.get()) + 8) {
            return true;
        }
    }
    return false;
}


} // namespace

// ------------------------------------------------------------------------------------------------
// Hooks
// ------------------------------------------------------------------------------------------------

// The world registries (block palette, feature registry) live on the level, whose own accessors are
// virtual and get devirtualised, so they are taken from the one call that receives the provider by
// reference and always runs while a level is configured.
LL_STATIC_HOOK(
    VanillaBiomesInitDefaultWorldGenComponentsHook,
    HookPriority::Normal,
    &VanillaBiomes::initDefaultWorldGenComponents,
    void,
    ::IWorldRegistriesProvider& registries
) {
    ::modapi::worldgen::BiomeRegistry::getInstance()._bindRegistries(registries);
    origin(registries);
}

// The registry itself is taken from a call the biome setup always makes: resolving a biome document
// by name. That hands ModAPI the registry before anything is registered into it.
using LookupByNameFn                       = ::Biome* (::BiomeRegistry::*)(::std::string const&);
inline LookupByNameFn const lookupByNameFn = static_cast<LookupByNameFn>(&::BiomeRegistry::lookupByName);

LL_TYPE_INSTANCE_HOOK(
    BiomeRegistryLookupByNameHook,
    HookPriority::Normal,
    ::BiomeRegistry,
    lookupByNameFn,
    ::Biome*,
    ::std::string const& name
) {
    auto* biome = origin(name);
    ::modapi::worldgen::BiomeRegistry::getInstance()._bindRegistry(*this);
    return biome;
}

// The climate cell lives in the overworld's multi-noise lattice: `OverworldBiomeBuilder::addBiomes`
// builds it and `ParameterList` is its only consumer, so appending here adds a cell.
LL_TYPE_INSTANCE_HOOK(
    ParameterListCtorHook,
    HookPriority::Normal,
    ::ParameterList,
    &::ParameterList::$ctor,
    void*,
    ::std::vector<::BiomeNoiseTarget> const& targets
) {
    auto& vec  = const_cast<::std::vector<::BiomeNoiseTarget>&>(targets);
    auto& self = ::modapi::worldgen::BiomeRegistry::getInstance();
    self._applyLattice(vec);
    auto* result = origin(targets);
    self._inspectLattice(*this);
    return result;
}

// Every chunk gets the surface builders of the biomes it contains. Overriding the entry here is the
// only supported way to install a builder that the engine's `isBestBuilder` pass cannot select.
LL_STATIC_HOOK(
    BuildChunkSurfaceHook,
    HookPriority::Normal,
    &::BiomeSurfaceSystem::buildChunkSurface,
    void,
    ::LevelChunk const&                                     levelChunk,
    ::BlockVolume&                                          blocks,
    ::SurfaceLevelCache const&                              surfaceLevelCache,
    ::DimensionHeightRange                                  dimensionHeightRange,
    ::std::array<float, 256> const*                         depthBuffer,
    ::ISurfaceBuilder::GlobalParameters const&              globalParams,
    ::BiomeSurfaceSystem::SurfaceBuilderParamsResult const& surfaceBuilderParamsResult
) {
    ::modapi::worldgen::BiomeRegistry::getInstance()._overrideSurfaceBuilders(surfaceBuilderParamsResult);
    origin(
        levelChunk,
        blocks,
        surfaceLevelCache,
        dimensionHeightRange,
        depthBuffer,
        globalParams,
        surfaceBuilderParamsResult
    );
}

struct BiomeRegistry::Impl {
    struct Entry {
        ::Biome*                      mBiome = nullptr;
        ::std::optional<BiomeClimate> mClimate;
        ::std::string                 mTemplateName;
        ::std::vector<::std::string>  mReplaceNames;
        unsigned short                mId = 0;
        ::std::string                 mSurfaceBuilderName;
        ::ISurfaceBuilder*            mSurfaceBuilder = nullptr;
    };

    ::BiomeRegistry*                   mRegistry   = nullptr;
    ::IWorldRegistriesProvider*        mRegistries = nullptr;
    bool                               mReady      = false;
    ::std::atomic<bool>                mEventRegistered{false};
    ll::DenseMap<::std::string, Entry> mEntries;
    bool                               mRefreshed = false;
    ll::memory::HookRegistrar<
        VanillaBiomesInitDefaultWorldGenComponentsHook,
        BiomeRegistryLookupByNameHook,
        ParameterListCtorHook,
        BuildChunkSurfaceHook>
        mHooks;
};

BiomeRegistry::BiomeRegistry() : pImpl(::std::make_unique<Impl>()) {}

BiomeRegistry::~BiomeRegistry() = default;

BiomeRegistry& BiomeRegistry::getInstance() {
    static BiomeRegistry instance;
    return instance;
}

bool BiomeRegistry::isReady() const noexcept { return pImpl->mReady; }

void BiomeRegistry::ensureEventRegistered() {
    (void)getInstance();
    core::ensureRegistryEventRegistered<BiomeReadyEvent>(getInstance().pImpl->mEventRegistered);

    // The `/locate biome` enum is built while the level configures its biomes, so the names are added
    // once the server is up. Reaching the command registry needs no hook: LL hands it out as a service.
    static bool listening = false;
    if (listening) return;
    listening = true;
    ll::event::EventBus::getInstance().emplaceListener<ll::event::server::ServerStartedEvent>(
        [](ll::event::server::ServerStartedEvent&) { BiomeRegistry::getInstance()._refreshLocateEnum(); }
    );
}

void BiomeRegistry::_bindRegistry(::BiomeRegistry& registry) {
    pImpl->mRegistry = &registry;
    _tryPublishReady();
}

void BiomeRegistry::_bindRegistries(::IWorldRegistriesProvider& registries) {
    pImpl->mRegistries = &registries;
    _tryPublishReady();
}

void BiomeRegistry::_tryPublishReady() {
    if (pImpl->mReady || pImpl->mRegistry == nullptr || pImpl->mRegistries == nullptr) return;
    pImpl->mReady = true;
    core::getLogger().info("BiomeRegistry: the vanilla biomes are in, custom biomes can be registered.");
    // Registration is still open here: the engine closes the registry in
    // `BiomeManager::_initializeServerBiomeRegistry`, which runs after the level and its registry are
    // handed to the biome setup.
    ll::event::EventBus::getInstance().publish(BiomeReadyEvent{*this});
    _refreshLocateEnum();
}

unsigned short BiomeRegistry::_allocateBiomeId() const {
    if (pImpl->mRegistry == nullptr) return 0;

    // The engine's counter is the only authority here: it already accounts for the ids behaviour packs
    // and other mods took (`mymod:test_biome` holds 30000, so the first one handed out was 30001), and
    // every registration below advances it. `lookupById` cannot be used to double check - it answers
    // for no id at all in this build, vanilla ones included.
    auto next = static_cast<unsigned short>(pImpl->mRegistry->mNextCustomBiomeId);
    if (next < kFirstCustomBiomeId || next == 0xFFFF) return 0;
    return next;
}

BiomeRegistry::ProductRef BiomeRegistry::registerBiome(::std::string_view identifier, BiomeProperty property) {
    return getInstance()._registerBiome(identifier, ::std::move(property));
}

BiomeRegistry::ProductRef BiomeRegistry::_registerBiome(::std::string_view identifier, BiomeProperty property) {
    if (pImpl->mRegistry == nullptr || pImpl->mRegistries == nullptr) return {};
    if (identifier.empty() || pImpl->mRegistry->lookupByName(::std::string{identifier}) != nullptr) return {};

    unsigned short id = 0;
    if (property.mId.has_value()) {
        id = property.mId->mValue;
        if (id < kFirstCustomBiomeId || id == 0xFFFF) return {};
    } else {
        id = _allocateBiomeId();
        if (id == 0) return {};
    }

    core::getLogger().info(
        "BiomeRegistry: '{}' gets id {} (next custom id was {})",
        identifier,
        id,
        static_cast<uint>(pImpl->mRegistry->mNextCustomBiomeId)
    );
    auto& biome = pImpl->mRegistry->_register(identifier, ::BiomeIdType{id});
    // `_register` builds the object but leaves the id field as the allocator left it, and that field
    // is what `/locate biome` compares against, so it is written here.
    const_cast<::BiomeIdType&>(biome.mId.get()).mValue = id;
    if (!property.mId.has_value()) {
        pImpl->mRegistry->mNextCustomBiomeId = static_cast<unsigned int>(id) + 1;
    }

    // A definition is taken from an existing biome instead of being built field by field: the engine
    // fills every optional block (chunk generation in particular) with real values, and what is left
    // is the few fields ModAPI owns. The template has to be named: it is where the components come
    // from.
    if (property.mTemplate.empty()) {
        core::getLogger().error("BiomeRegistry: the biome '{}' names no template biome to build from", identifier);
        return {};
    }
    auto* source = pImpl->mRegistry->lookupByName(property.mTemplate);
    if (source == nullptr) {
        core::getLogger().error(
            "BiomeRegistry: the biome '{}' cannot be built from '{}': no such biome",
            identifier,
            property.mTemplate
        );
        return {};
    }

    ::BiomeStringList     strings{};
    ::BiomeDefinitionData definition{};
    ::VanillaBiomeSerialization::serializeBiomeToData(
        *source,
        definition,
        true,
        pImpl->mRegistry->mTagRegistry.get(),
        pImpl->mRegistries->getFeatureRegistry(),
        strings
    );

    definition.mTemperature       = 0.8f;
    definition.mDownfall          = 0.4f;
    definition.mFoliageSnow       = 0.f;
    definition.mDepth             = 0.1f;
    definition.mScale             = 0.2f;
    definition.mMapWaterColorARGB = static_cast<int>(0xFF000000);
    definition.mRain              = true;

    if (definition.mChunkGenData.get().has_value()) {
        definition.mChunkGenData.get()->mSurfaceBuilderData.get() = makeSurface(property);
    }

    ::VanillaBiomeSerialization::applySerializedDataToBiome(
        definition,
        biome,
        pImpl->mRegistry->mTagRegistry.get(),
        pImpl->mRegistries->getBlockPalette(),
        *pImpl->mRegistry,
        pImpl->mRegistries->getFeatureRegistry(),
        strings
    );

    if (!property.mComponents.empty()) {
        auto& storage                  = biome.mBiomeComponentStorage.get();
        storage.mAllowAddingComponents = true;
        for (auto& component : property.mComponents) {
            if (!component.mCreate) continue;
            storage.mComponentList.get().emplace_back(component.mKey, component.mCreate());
        }
    }

    return _remember(identifier, biome, id, ::std::move(property));
}

bool BiomeRegistry::configureBiome(::std::string_view identifier, BiomeProperty property) {
    if (pImpl->mRegistry == nullptr) return false;
    auto* biome = pImpl->mRegistry->lookupByName(::std::string{identifier});
    if (biome == nullptr) return false;
    // A definition is never applied here: the biome already has its components and adding them a
    // second time is an engine assertion.
    // The id is not known for a biome that was not registered here; 0 keeps it out of the enum refresh.
    auto ref = _remember(identifier, *biome, 0, ::std::move(property));
    return ref.has_value();
}

BiomeRegistry::ProductRef BiomeRegistry::getBiome(::std::string_view identifier) const {
    if (pImpl->mRegistry == nullptr) return {};
    return pImpl->mRegistry->lookupByName(::std::string{identifier});
}


BiomeRegistry::ProductRef
BiomeRegistry::_remember(::std::string_view identifier, ::Biome& biome, unsigned short id, BiomeProperty property) {
    if (pImpl->mEntries.contains(identifier)) return {};
    Impl::Entry entry{};
    entry.mBiome        = &biome;
    entry.mId           = id;
    entry.mTemplateName = property.mTemplate;
    if (auto const* climate = ::std::get_if<BiomeClimate>(&property.mClimate); climate != nullptr) {
        // Cells of its own: appended to the lattice.
        entry.mClimate = *climate;
    } else if (
        auto const* replacement = ::std::get_if<BiomeProperty::Replacement>(&property.mClimate); replacement != nullptr
    ) {
        // Cells of other biomes: every target's lattice cells are rewritten, an empty list stands for
        // the template. Only a whole replacement is reachable through the lattice.
        if (replacement->mAmount < 1.f || replacement->mNoiseFrequencyScale != 1.f) {
            core::getLogger().error(
                "BiomeRegistry: '{}' asks for a partial replacement (amount {}, noise {}); only a whole "
                "replacement is supported",
                identifier,
                replacement->mAmount,
                replacement->mNoiseFrequencyScale
            );
            return {};
        }
        if (replacement->mDimension != "minecraft:overworld") {
            core::getLogger().error(
                "BiomeRegistry: '{}' replaces biomes in '{}'; only 'minecraft:overworld' is supported",
                identifier,
                replacement->mDimension
            );
            return {};
        }
        entry.mReplaceNames = replacement->mTargetBiomes;
        if (entry.mReplaceNames.empty()) entry.mReplaceNames.push_back(property.mTemplate);
    }
    // Resolved lazily: the surface registry is bound after the biome registry, so a builder may not
    // exist yet when the biome is registered.
    if (auto const* name = ::std::get_if<::std::string>(&property.mSurface); name != nullptr) {
        entry.mSurfaceBuilderName = *name;
    }
    pImpl->mEntries.emplace(::std::string{identifier}, ::std::move(entry));
    return biome;
}

void BiomeRegistry::_applyLattice(::std::vector<::BiomeNoiseTarget>& targets) {
    if (pImpl->mEntries.empty()) return;
    size_t added = 0;
    for (auto& [name, entry] : pImpl->mEntries) {
        if (entry.mBiome == nullptr) continue;

        // A property that claims cells of its own gets them appended; a `Replacement` instead rewrites
        // the units of the biomes it names.
        if (entry.mReplaceNames.empty() == false) {
            for (auto const& name : entry.mReplaceNames) {
                auto* replaced = pImpl->mRegistry != nullptr ? pImpl->mRegistry->lookupByName(name) : nullptr;
                if (replaced == nullptr) continue;
                for (auto& target : targets) {
                    if (target.data == replaced) target.data = entry.mBiome;
                }
            }
            continue;
        }

        if (!entry.mClimate.has_value()) continue;
        auto const& climate = *entry.mClimate;

        // `ClimateUtils::Parameter(float, float)` does the fixed point conversion the engine uses.
        ::ClimateParameters parameters{
            ::ClimateUtils::Parameter{climate.mTemperatureMin,     climate.mTemperatureMax    },
            ::ClimateUtils::Parameter{climate.mHumidityMin,        climate.mHumidityMax       },
            ::ClimateUtils::Parameter{climate.mContinentalnessMin, climate.mContinentalnessMax},
            ::ClimateUtils::Parameter{climate.mErosionMin,         climate.mErosionMax        },
            ::ClimateUtils::Parameter{climate.mDepthMin,           climate.mDepthMax          },
            ::ClimateUtils::Parameter{climate.mWeirdnessMin,       climate.mWeirdnessMax      },
            climate.mOffset
        };
        targets.push_back(::BiomeNoiseTarget{entry.mBiome, parameters});
        ++added;
    }
    if (added != 0) {
        core::getLogger().info("BiomeRegistry: {} lattice cells appended, {} in total", added, targets.size());
    }
}

void BiomeRegistry::dumpComponentKeys() {
    auto& self = getInstance();
    if (self.pImpl->mRegistry == nullptr) {
        core::getLogger().error("BiomeRegistry: no registry yet, nothing to dump");
        return;
    }

    for (auto& [hash, biome] : self.pImpl->mRegistry->mBiomesByName.get()) {
        if (!biome) continue;
        ::std::string keys;
        for (auto& [key, component] : biome->mBiomeComponentStorage.get().mComponentList.get()) {
            if (!keys.empty()) keys += ", ";
            keys += ::std::to_string(key.value);
        }
        core::getLogger().info(
            "BiomeRegistry: {} | {} | {}",
            biome->mHash.get().getString(),
            biome->mBiomeComponentStorage.get().mComponentList.get().size(),
            keys
        );
    }
}

void BiomeRegistry::_refreshLocateEnum() {
    auto commandRegistry = ::ll::service::getCommandRegistry();
    if (!commandRegistry) return;
    auto& registry = *commandRegistry;

    // The biome argument of `locate` is the enum the engine filed under the name "Biome"; a marker
    // byte may prefix it, so the name is matched by suffix.
    uint index = 0;
    bool found = false;
    for (auto& [name, candidate] : registry.mEnumLookup) {
        if (name.size() >= 5 && name.compare(name.size() - 5, 5, "Biome") == 0) {
            index = candidate;
            found = true;
            break;
        }
    }
    if (!found) return;
    auto& engineEnum = registry.mEnums[index];

    ::std::vector<::std::pair<::std::string, uint64>> names;
    for (auto& [name, entry] : pImpl->mEntries) {
        if (entry.mId == 0) continue;
        names.emplace_back(name, entry.mId);
    }
    if (names.empty() || pImpl->mRefreshed) return;
    pImpl->mRefreshed = true;

    // The engine's own conversion: it turns names into symbols the parser knows, which a raw table
    // write does not (a symbol carries type bits).
    registry._addEnumValuesInternal(engineEnum.name, names, engineEnum.type, engineEnum.parse);

    auto after = registry.mEnumLookup.find("Biome");
    core::getLogger().info(
        "BiomeRegistry: the /locate biome enum has {} values (index {} -> {})",
        registry.mEnums[index].values.size(),
        index,
        after != registry.mEnumLookup.end() ? static_cast<int>(after->second) : -1
    );
}

void BiomeRegistry::_inspectLattice(::ParameterList const& list) {
    auto const& tree = list.mTree.get();
    if (!tree.has_value()) return;

    size_t leaves = 0;
    size_t ours   = 0;
    auto   walk   = [&](auto&& self, ::RTree::Node const& node) -> void {
        auto const& biome = node.mBiome.get();
        if (!biome.has_value()) {
            for (auto const& child : node.mChildren.get()) self(self, child);
            return;
        }
        ++leaves;
        for (auto& [name, entry] : pImpl->mEntries) {
            if (entry.mBiome == biome.value()) {
                ++ours;
                return;
            }
        }
    };
    walk(walk, tree->mRoot.get());

    core::getLogger().info("BiomeRegistry: the engine's tree has {} leaves, {} of them ModAPI's", leaves, ours);
}

void BiomeRegistry::_overrideSurfaceBuilders(::BiomeSurfaceSystem::SurfaceBuilderParamsResult const& params) {
    if (pImpl->mEntries.empty()) return;
    auto& mutableParams = const_cast<::BiomeSurfaceSystem::SurfaceBuilderParamsResult&>(params);
    for (auto& param : mutableParams.mSurfaceBuilderParams.get()) {
        for (auto& [name, entry] : pImpl->mEntries) {
            if (entry.mSurfaceBuilder == nullptr && !entry.mSurfaceBuilderName.empty()) {
                entry.mSurfaceBuilder = SurfaceBuilderRegistry::getInstance().getBuilder(entry.mSurfaceBuilderName);
            }
            if (entry.mSurfaceBuilder == nullptr || entry.mBiome == nullptr) continue;
            if (ownsSurfaceData(*entry.mBiome, param.mSurfaceBuilderData)) {
                param.mSurfaceBuilder = entry.mSurfaceBuilder;
                break;
            }
        }
    }
}

BiomeRegistry& BiomeReadyEvent::registry() const { return mRegistry; }

BiomeClimate BiomeClimate::fullRange() { return {}; }

BiomeClimate BiomeClimate::underground() {
    BiomeClimate climate{};
    climate.mDepthMin = 0.2f;
    climate.mDepthMax = 0.8999f;
    return climate;
}

} // namespace modapi::inline worldgen

// The engine declares these and does not export them, so whichever translation unit needs them
// supplies the definition - the same thing `BlockRegistry.cpp` does for the block definition types.
namespace SharedTypes::v1_26_30 {
NoiseDescriptor& NoiseDescriptor::operator=(NoiseDescriptor const&) = default;
} // namespace SharedTypes::v1_26_30
