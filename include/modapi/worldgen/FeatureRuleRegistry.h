#pragma once
#include "modapi/Macros.h"
#include "modapi/Registry.h"
#include "modapi/worldgen/base/ICustomFeatureRule.h"
#include <concepts>
#include <functional>
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

class FeatureRuleRegistry;

// Published right after the features of a level were registered, per level, so a rule can refer to the feature it
// places. `FeatureReadyEvent` has already run when this fires.
class FeatureRuleReadyEvent final : public ll::event::Event {
    FeatureRuleRegistry& mRegistry;

public:
    constexpr explicit FeatureRuleReadyEvent(FeatureRuleRegistry& registry) : mRegistry(registry) {}

    MOD_NDAPI FeatureRuleRegistry& registry() const;
};

// What to register, in one place, mirroring `BlockRegistration`: the identifier, the feature whose placement this rule
// drives, the constructor arguments of `Entry` and the passes the rule takes part in. The call site names the type:
//   DeferredRegister<FeatureRuleRegistry, MyRule> gRule{
//       FeatureRuleRegistration<>{
//           .mIdentifier = "mymod:my_rule", .mPlacesFeature = "mymod:my_feature", .mPasses = {"mymod:trees"}
//       }
//   };
template <class... Args>
struct FeatureRuleRegistration {
    std::string mIdentifier;
    // The registered feature that does the placing: the rule only yields the positions it is called with.
    std::string         mPlacesFeature;
    std::tuple<Args...> mArguments;
    // The passes the rule takes part in; empty means every pass the level configures.
    std::vector<std::string> mPasses;
};

class FeatureRuleRegistry {
    struct Impl;
    std::unique_ptr<Impl> pImpl;

public:
    using EventType  = FeatureRuleReadyEvent;
    using Product    = ::IFeature;
    using ProductRef = optional_ref<::IFeature>;

public:
    FeatureRuleRegistry();
    FeatureRuleRegistry& operator=(FeatureRuleRegistry const&) = delete;
    FeatureRuleRegistry(FeatureRuleRegistry const&)            = delete;

public:
    MOD_NDAPI static FeatureRuleRegistry& getInstance();

    // True once the features of a level have been registered.
    MOD_NDAPI bool isReady() const noexcept;

    // Registers the ready event emitter (idempotent).
    MOD_API static void ensureEventRegistered();

    template <class Entry>
    static constexpr bool isEntry = std::derived_from<Entry, ICustomFeatureRule>;

    template <class Entry, class... Args>
        requires isEntry<Entry>
    ProductRef registerEntry(FeatureRuleRegistration<Args...> registration) {
        try {
            auto rule = std::apply(
                [](auto&&... args) -> std::unique_ptr<ICustomFeatureRule> {
                    return std::make_unique<Entry>(std::forward<decltype(args)>(args)...);
                },
                std::move(registration.mArguments)
            );
            return _registerRule(
                registration.mIdentifier,
                registration.mPlacesFeature,
                registration.mPasses,
                std::move(rule)
            );
        } catch (...) {
            return {};
        }
    }

    // Registers one rule into the currently bound registry.
    MOD_NDAPI ProductRef _registerRule(
        std::string const&                  identifier,
        std::string const&                  placesFeature,
        std::vector<std::string> const&     passes,
        std::unique_ptr<ICustomFeatureRule> rule
    );

    // Binds the registry the features of a level went into and publishes `FeatureRuleReadyEvent` for it.
    MOD_API void _bindRegistry(::FeatureRegistry& registry);
};

} // namespace modapi::inline worldgen

static_assert(modapi::Registry<modapi::FeatureRuleRegistry>);
