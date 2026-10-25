#pragma once
#include "modapi/Macros.h"
#include "modapi/Registration.h"
#include "modapi/Registry.h"
#include "modapi/worldgen/base/ICustomFeatureRule.h"
#include <concepts>
#include <ll/api/event/Event.h>
#include <mc/deps/core/utility/optional_ref.h>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class FeatureRegistry;

namespace modapi::inline worldgen {

class FeatureRuleRegistry;

class FeatureRuleReadyEvent final : public ll::event::Event {
    FeatureRuleRegistry& mRegistry;

public:
    constexpr explicit FeatureRuleReadyEvent(FeatureRuleRegistry& registry) : mRegistry(registry) {}

    MOD_NDAPI FeatureRuleRegistry& registry() const;
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

    MOD_NDAPI bool isReady() const noexcept;

    MOD_API static void ensureEventRegistered();

    template <class Entry>
    static constexpr bool isEntry = std::derived_from<Entry, ICustomFeatureRule>;

    template <class Entry>
    static ClassRegistrationBuilder<FeatureRuleRegistry, Entry> builder(std::string identifier) {
        return ClassRegistrationBuilder<FeatureRuleRegistry, Entry>{std::move(identifier)};
    }

    template <class Entry, class... Args>
        requires isEntry<Entry>
    ProductRef registerEntry(ClassRegistration<Entry, Args...> registration) {
        try {
            auto rule = std::apply(
                [](auto&&... args) -> std::unique_ptr<ICustomFeatureRule> {
                    return std::make_unique<Entry>(std::forward<decltype(args)>(args)...);
                },
                registration.mArguments
            );
            auto places = rule->placesFeature();
            auto passes = rule->passes();
            return _registerRule(registration.mIdentifier, places, passes, std::move(rule));
        } catch (...) {
            return {};
        }
    }

    MOD_NDAPI ProductRef _registerRule(
        std::string const&                  identifier,
        std::string const&                  placesFeature,
        std::vector<std::string> const&     passes,
        std::unique_ptr<ICustomFeatureRule> rule
    );

    MOD_API void _bindRegistry(::FeatureRegistry& registry);
};

} // namespace modapi::inline worldgen

static_assert(modapi::Registry<modapi::FeatureRuleRegistry>);
