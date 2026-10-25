#pragma once
#include "modapi/Macros.h"
#include "modapi/Registry.h"
#include <functional>
#include <ll/api/event/Event.h>
#include <mc/deps/core/utility/optional_ref.h>
#include <mc/deps/core/utility/typeid_t.h>
#include <mc/world/level/biome/BiomeIdType.h>
#include <mc/world/level/biome/components/BiomeComponentBase.h>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

class Biome;
class Level;
class BiomeRegistry;
class IWorldRegistriesProvider;
class ParameterList;
struct BiomeNoiseTarget;
namespace BiomeSurfaceSystem {
struct SurfaceBuilderParamsResult;
}

namespace modapi::inline worldgen {

class BiomeRegistry;

// One cell of the overworld's multi-noise biome lattice. The engine stores the six dimensions as
// fixed point (value x 10000) inside `ClimateParameters`; this is the same thing in normalized
// floats, so [-1, 1] means "the whole axis".
struct BiomeClimate {
    float mTemperatureMin = -1.f, mTemperatureMax = 1.f;
    float mHumidityMin = -1.f, mHumidityMax = 1.f;
    float mContinentalnessMin = -1.f, mContinentalnessMax = 1.f;
    float mErosionMin = -1.f, mErosionMax = 1.f;
    float mDepthMin = -1.f, mDepthMax = 1.f;
    float mWeirdnessMin = -1.f, mWeirdnessMax = 1.f;
    float mOffset = 0.f;

public:
    // Claims every cell. Combined with a biome that replaced a vanilla one, this is how a mod takes
    // over that biome's whole share of the world.
    MOD_NDAPI static BiomeClimate fullRange();

    // The cells the engine's own underground biome pass uses: full range except depth [0.2, 0.8999].
    MOD_NDAPI static BiomeClimate underground();
};

// What a biome registration carries. The biome's components (climate numbers, surface materials,
// generation rules) cannot be created from a native mod - `BiomeComponentStorage::addComponent` and
// `BiomeDefinitionData`'s constructors are not exported - so ModAPI only puts the biome into the
// registry and gives it the two things that are reachable from C++: a climate cell (placement) and
// a surface builder (what the terrain looks like).
struct BiomeProperty {
    ::std::optional<::BiomeIdType> mId; // empty => ModAPI allocates one above 30000

    // The biome whose definition ModAPI copies. Required: the engine's biome components cannot be
    // created from a native mod, so an existing biome has to supply them.
    //
    // This is not the engine's `BiomeReplacement` (`minecraft:replace_biomes`): that one swaps
    // biomes at runtime by noise, per dimension, through `ReplaceBiomesComponent`. This one only
    // says where the components and, by default, the climate cells come from.
    ::std::string mTemplate;

    // The engine's `::BiomeReplacement` (`minecraft:replace_biomes`), field for field: the biomes this
    // one replaces, the dimension it does that in, and how much of them it takes.
    //
    // ModAPI applies it to the overworld's multi-noise lattice, which is the only part of the world
    // the biome source is built from here: every lattice cell of a target biome is handed to this
    // biome, so `mAmount` and `mNoiseFrequencyScale` can only be honoured at their default (the whole
    // replacement, evenly). A registration that asks for a partial one is refused rather than
    // silently registered as a full takeover.
    struct Replacement {
        // Empty means "the template's cells".
        ::std::vector<::std::string> mTargetBiomes;
        ::std::string                mDimension           = "minecraft:overworld";
        float                        mAmount              = 1.f;
        float                        mNoiseFrequencyScale = 1.f;
    };

    // Where the climate cells come from: replace another biome's cells, or claim cells of its own
    // (ModAPI appends them to the overworld's lattice). Defaults to replacing the template's cells.
    using Climate = ::std::variant<Replacement, BiomeClimate>;

    Climate mClimate{};

    // One entry of the biome's component storage, filed under the key the engine assigned to that
    // component's type. `mCreate` is called for every registration pass, so a replayed registration
    // gets a fresh object instead of one shared by two biomes.
    //
    // The keys are handed out by the engine at start up and are not exported; `dumpComponentKeys()`
    // prints them per biome, which is how the key of a component type is learned for a given build.
    // Ids no engine component uses (any value far above the vanilla range, say 60000) are safe to
    // take: the engine never looks them up, only ModAPI's own code reads them.
    struct Component {
        ::Bedrock::typeid_t<void>                                  mKey;
        ::std::function<::std::unique_ptr<::BiomeComponentBase>()> mCreate;
    };

    ::std::vector<Component> mComponents;

    // Either the preset the engine resolves on its own, or the name of a builder registered in
    // `SurfaceBuilderRegistry`. A custom builder replaces the preset's behaviour for this biome.
    enum class SurfacePreset : unsigned char {
        Overworld,
        Swamp,
        FrozenOcean,
        TheEnd,
    };
    using Surface = ::std::variant<SurfacePreset, ::std::string>;

    Surface mSurface = SurfacePreset::Overworld;
};
// What to register: the identifier and the property. A biome has no user class to name - the engine
// builds the `::Biome` by copying the template - so the registration itself is the entry type:
//   DeferredRegister<BiomeRegistry, BiomeRegistration> gBiome{
//       BiomeRegistration{
//           .mIdentifier = "mymod:my_biome", .mProperty = {.mTemplate = "minecraft:plains"}
//       }
//   };
struct BiomeRegistration {
    std::string   mIdentifier;
    BiomeProperty mProperty;
};

// Published after `VanillaBiomes::initBiomes` has run and before the engine closes the biome
// registry, so entries registered in a listener still reach every client and every level. Behaviour
// packs are loaded after this point; a biome that a pack defines is reached through
// `getBiome`/`configureBiome` instead.
class BiomeReadyEvent final : public ll::event::Event {
    BiomeRegistry& mRegistry;

public:
    constexpr explicit BiomeReadyEvent(BiomeRegistry& registry) : mRegistry(registry) {}

    MOD_API BiomeRegistry& registry() const;
};

// Custom biomes: the registry entry is C++, the climate cell is C++, the surface is C++.
//
// Registration has to happen inside `BiomeReadyEvent`, which is ModAPI's name for "the vanilla biome
// pass has finished and the registry is still open":
//
//   ll::event::EventBus::getInstance().emplaceListener<modapi::worldgen::BiomeReadyEvent>(
//       [](auto& event) {
//           event.registry().registerBiome("mymod:my_biome", {
//               .mTemplate = "minecraft:plains",                         // the template
//               .mSurface = ::std::string{"mymod:my_surface"},           // or a SurfacePreset
//           });
//       }
//   );
//
// `mymod:my_surface` is an `ICustomSurfaceBuilder` registered in `SurfaceBuilderRegistry`, which is
// ready from `SurfaceBuilderReadyEvent` on - before any biome is registered.
class BiomeRegistry {
public:
    struct Impl;
    std::unique_ptr<Impl> pImpl;

public:
    using EventType  = BiomeReadyEvent;
    using Product    = ::Biome;
    using ProductRef = optional_ref<::Biome>;

public:
    BiomeRegistry();
    BiomeRegistry& operator=(BiomeRegistry const&) = delete;
    BiomeRegistry(BiomeRegistry const&)            = delete;
    ~BiomeRegistry();

public:
    MOD_NDAPI static BiomeRegistry& getInstance();

    // True once the engine's registry is known and still accepting entries.
    [[nodiscard]] MOD_NDAPI bool isReady() const noexcept;

    // Registers the ready event emitter (idempotent).
    MOD_API static void ensureEventRegistered();

    template <class Entry>
    static constexpr bool isEntry = ::std::same_as<Entry, BiomeRegistration>;

    // What `DeferredRegister` calls; the registration is the entry.
    template <class Entry>
        requires isEntry<Entry>
    ProductRef registerEntry(Entry registration) {
        return registerBiome(registration.mIdentifier, ::std::move(registration.mProperty));
    }

    // Registers a new biome and applies `property`. The identifier has to be free: a name that
    // already belongs to a biome is refused rather than registered on top of, because the engine
    // keeps the first one.
    MOD_NDAPI ProductRef registerBiome(std::string_view identifier, BiomeProperty property = {});

    // Applies `property` to a biome that already exists - one a behaviour pack defined, or one
    // another mod registered. That is the supported way to give a pack biome a climate cell or a
    // surface builder.
    MOD_NDAPI bool configureBiome(std::string_view identifier, BiomeProperty property);

    // The biome registered under `identifier`, or nothing.
    [[nodiscard]] MOD_NDAPI ProductRef getBiome(std::string_view identifier) const;


    // The registry and the world registries arrive through different engine calls; the ready event
    // is published once both are known. `IWorldRegistriesProvider` is the level's own interface
    // (it is what hands ModAPI the block palette and the feature registry).
    MOD_API void _bindRegistry(::BiomeRegistry& registry);
    MOD_API void _bindRegistries(::IWorldRegistriesProvider& registries);

    // Next id ModAPI would hand out; 0 when it is out of the reserved range.
    MOD_NDAPI unsigned short _allocateBiomeId() const;

    MOD_NDAPI ProductRef _registerBiome(std::string_view identifier, BiomeProperty property);

    // Logs every component the engine installed on every biome, with the `typeid_t` key the engine
    // filed it under. The ids are handed out per process and are not exported, so this is the only
    // way to learn the key of a component type.
    MOD_API static void dumpComponentKeys();

    // Remembers what a registration asked for: the biome, its id, its climate cell and its surface
    // builder.
    MOD_NDAPI ProductRef
    _remember(std::string_view identifier, ::Biome& biome, unsigned short id, BiomeProperty property);

    // Applies every remembered biome to the lattice the engine is building: takes over the cells of
    // a replaced biome, then appends the ones a registration asked for itself.
    MOD_API void _applyLattice(::std::vector<::BiomeNoiseTarget>& targets);

    // Reports what the engine's tree kept of the lattice ModAPI handed it.
    MOD_API void _inspectLattice(::ParameterList const& list);

    // Replaces the surface builder of a chunk's entries that belong to a remembered biome.
    MOD_API void _overrideSurfaceBuilders(::BiomeSurfaceSystem::SurfaceBuilderParamsResult const& params);

    MOD_API void _tryPublishReady();

    // Adds the registered biomes to the `/locate biome` enum. The engine builds that enum once, so
    // without this only the biomes that existed at that moment would be addressable by name.
    MOD_API void _refreshLocateEnum();
};

} // namespace modapi::inline worldgen

static_assert(modapi::Registry<modapi::BiomeRegistry>);
