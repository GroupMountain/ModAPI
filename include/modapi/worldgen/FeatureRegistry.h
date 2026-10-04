#pragma once
#include "modapi/Macros.h"
#include "modapi/Registry.h"
#include "modapi/worldgen/base/ICustomFeature.h"
#include <concepts>
#include <functional>
#include <ll/api/coro/Generator.h>
#include <ll/api/event/Event.h>
#include <mc/deps/core/utility/optional_ref.h>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

class FeatureRegistry;

namespace modapi::inline worldgen {

class FeatureRegistry;

// Published from `VanillaFeatures::registerFeatures`, which the game module calls for every level
// it configures. Deferred entries are built and registered into the `FeatureRegistry` the event
// carries, so custom features are part of every configured level.
class FeatureReadyEvent final : public ll::event::Event {
    FeatureRegistry& mRegistry;

public:
    constexpr explicit FeatureReadyEvent(FeatureRegistry& registry) : mRegistry(registry) {}

    MOD_NDAPI FeatureRegistry& registry() const;
};

class FeatureRegistry {
    struct Impl;
    std::unique_ptr<Impl> pImpl;

public:
    using EventType  = FeatureReadyEvent;
    using Product    = ::IFeature;
    using ProductRef = optional_ref<::IFeature>;

public:
    FeatureRegistry();
    FeatureRegistry& operator=(FeatureRegistry const&) = delete;
    FeatureRegistry(FeatureRegistry const&)            = delete;

public:
    MOD_NDAPI static FeatureRegistry& getInstance();

    // True once vanilla registered its features for a level.
    MOD_NDAPI bool isReady() const noexcept;

    // Registers the ready event emitter (idempotent).
    MOD_API static void ensureEventRegistered();

    template <class Entry>
    static constexpr bool isEntry = std::derived_from<Entry, ::IFeature>;

    // What to register, in one place, mirroring `BlockRegistration`: the identifier and the constructor arguments
    // of `Entry`. The call site names the type, exactly as it does for a block:
    //   DeferredRegister<FeatureRegistry, MyFeature> gFeature{
    //       FeatureRegistration<>{ .mIdentifier = "mymod:my_feature", .mArguments = {} }
    //   };
    template <class... Args>
    struct FeatureRegistration {
        std::string         mIdentifier;
        std::tuple<Args...> mArguments;
    };

    template <class Entry, class... Args>
        requires isEntry<Entry>
    ProductRef registerEntry(FeatureRegistration<Args...> registration) {
        try {
            auto feature = std::apply(
                [](auto&&... args) -> std::unique_ptr<::IFeature> {
                    return std::make_unique<Entry>(std::forward<decltype(args)>(args)...);
                },
                std::move(registration.mArguments)
            );
            return _registerFeature(registration.mIdentifier, std::move(feature));
        } catch (...) {
            return {};
        }
    }

    // Single shot registration of an already built feature; not replayed for later levels, use
    // `DeferredRegister` for that.
    MOD_NDAPI ProductRef registerFeature(std::string_view identifier, std::unique_ptr<IFeature> feature);

    // Binds the registry vanilla is filling and publishes `FeatureReadyEvent` for it.
    MOD_API void _bindRegistry(::FeatureRegistry& registry);

    // Registers one feature into the currently bound registry.
    MOD_NDAPI ProductRef _registerFeature(std::string_view identifier, std::unique_ptr<IFeature> feature);

    // Registers the features of the rule based API into the given registry.
    MOD_API void _applyFeatureFactories(::FeatureRegistry& registry);
};

} // namespace modapi::inline worldgen

static_assert(modapi::Registry<modapi::FeatureRegistry>);
