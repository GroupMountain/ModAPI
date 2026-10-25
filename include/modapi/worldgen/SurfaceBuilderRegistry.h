#pragma once
#include "modapi/Macros.h"
#include "modapi/Registry.h"
#include "modapi/worldgen/base/ICustomSurfaceBuilder.h"
#include <ll/api/event/Event.h>
#include <mc/deps/core/utility/optional_ref.h>
#include <mc/world/level/LevelSeed64.h>
#include <mc/world/level/biome/SurfaceBuilderType.h>
#include <mc/world/level/biome/surface/ISurfaceBuilder.h>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

class SurfaceBuilderRegistry;

namespace modapi::inline worldgen {

class SurfaceBuilderRegistry;

// Published once the engine has filled its surface builder registry with the vanilla builders, i.e.
// after `VanillaBiomes::initSurfaceBuilders` and before any chunk asks for a surface. Entries added
// later (from a deferred register, say) still work, they just miss the event.
class SurfaceBuilderReadyEvent final : public ll::event::Event {
    SurfaceBuilderRegistry& mRegistry;

public:
    constexpr explicit SurfaceBuilderReadyEvent(SurfaceBuilderRegistry& registry) : mRegistry(registry) {}

    MOD_API SurfaceBuilderRegistry& registry() const;
};

// What to register, mirroring `FeatureRegistration`: the identifier and the constructor arguments of
// `Entry`. The call site names the type:
//   DeferredRegister<SurfaceBuilderRegistry, MyBuilder> gBuilder{
//       SurfaceBuilderRegistration<>{ .mIdentifier = "mymod:my_builder", .mArguments = {} }
//   };
template <class... Args>
struct SurfaceBuilderRegistration {
    std::string         mIdentifier;
    std::tuple<Args...> mArguments;
};

// Surface builders written in C++. A builder is registered once and can then be named by any number
// of biomes; the engine's own registry keys builders by `SurfaceBuilderType`, so ModAPI allocates a
// key above the vanilla range (0..7) for every entry it adds.
class SurfaceBuilderRegistry {
public:
    struct Impl;
    std::unique_ptr<Impl> pImpl;

public:
    using EventType  = SurfaceBuilderReadyEvent;
    using Product    = ::ISurfaceBuilder;
    using ProductRef = optional_ref<::ISurfaceBuilder>;

public:
    SurfaceBuilderRegistry();
    SurfaceBuilderRegistry& operator=(SurfaceBuilderRegistry const&) = delete;
    SurfaceBuilderRegistry(SurfaceBuilderRegistry const&)            = delete;
    ~SurfaceBuilderRegistry();

public:
    MOD_NDAPI static SurfaceBuilderRegistry& getInstance();

    // True once the engine's registry is known, i.e. from `SurfaceBuilderReadyEvent` on.
    [[nodiscard]] MOD_NDAPI bool isReady() const noexcept;

    // Registers the ready event emitter (idempotent).
    MOD_API static void ensureEventRegistered();

    template <class Entry>
    static constexpr bool isEntry = std::derived_from<Entry, ICustomSurfaceBuilder>;

    // Builds `Entry` from the stored arguments and registers it; what `DeferredRegister` calls.
    template <class Entry, class... Args>
        requires isEntry<Entry>
    ProductRef registerEntry(SurfaceBuilderRegistration<Args...> registration) {
        try {
            auto builder = std::apply(
                [](auto&&... args) -> std::unique_ptr<::ISurfaceBuilder> {
                    return std::make_unique<Entry>(::std::forward<decltype(args)>(args)...);
                },
                ::std::move(registration.mArguments)
            );
            return registerBuilder(registration.mIdentifier, ::std::move(builder));
        } catch (...) {
            return {};
        }
    }

    // Hands `builder` to the engine under a ModAPI allocated key. A name that is already taken is
    // refused: the engine keeps the first entry for a key and the caller would be left holding an
    // object the engine never references.
    MOD_NDAPI ProductRef registerBuilder(std::string_view identifier, std::unique_ptr<::ISurfaceBuilder> builder);

    // The builder registered under `identifier`, or nothing.
    [[nodiscard]] MOD_NDAPI ProductRef getBuilder(std::string_view identifier) const;

    // Binds the engine registry this instance forwards to, and publishes `SurfaceBuilderReadyEvent`.
    MOD_API void _bindRegistry(::SurfaceBuilderRegistry& registry);

    MOD_NDAPI ProductRef _registerBuilder(std::string_view identifier, std::unique_ptr<::ISurfaceBuilder> builder);
};

} // namespace modapi::inline worldgen

static_assert(modapi::Registry<modapi::SurfaceBuilderRegistry>);
