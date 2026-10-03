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
    using CustomFeatureRule =
        std::function<ll::coro::Generator<BlockPos>(BlockHelper const& helper, BlockPos const& pos, Random& random)>;

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

    // The identifier is the first argument, then the constructor arguments of `Entry`:
    //   DeferredRegister<FeatureRegistry, MyFeature> gFeature("mymod:my_feature", ctorArgs...);
    template <class Entry, class... Args>
        requires isEntry<Entry>
    ProductRef registerEntry(std::string_view identifier, Args&&... args) {
        try {
            return _registerFeature(identifier, std::make_unique<Entry>(std::forward<Args>(args)...));
        } catch (...) {
            return {};
        }
    }

    // Single shot registration of an already built feature; not replayed for later levels, use
    // `DeferredRegister` for that.
    MOD_NDAPI ProductRef registerFeature(std::string_view identifier, std::unique_ptr<IFeature> feature);

    // Feature rule built from a ModAPI callback. The feature is rebuilt for every level, so the
    // returned product is the one registered for the pass that was running, if any.
    MOD_NDAPI ProductRef
    registerFeatureRule(std::string_view identifier, std::vector<std::string> const& passes, CustomFeatureRule rule);

    // Binds the registry vanilla is filling and publishes `FeatureReadyEvent` for it.
    MOD_API void _bindRegistry(::FeatureRegistry& registry);

    // Registers one feature into the currently bound registry.
    MOD_NDAPI ProductRef _registerFeature(std::string_view identifier, std::unique_ptr<IFeature> feature);

    // Registers the features of the rule based API into the given registry.
    MOD_API void _applyFeatureFactories(::FeatureRegistry& registry);
};

} // namespace modapi::inline worldgen

static_assert(modapi::Registry<modapi::FeatureRegistry>);
