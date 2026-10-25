#pragma once
#include "modapi/Macros.h"
#include "modapi/Registration.h"
#include "modapi/Registry.h"
#include <ll/api/event/Event.h>
#include <mc/deps/core/utility/optional_ref.h>
#include <mc/world/level/storage/GameRule.h>
#include <memory>
#include <string>
#include <utility>
#include <variant>

class GameRules;

namespace modapi::inline gamerule {

class GameRuleRegistry;

class GameRuleReadyEvent final : public ll::event::Event {
    GameRuleRegistry& mRegistry;

public:
    constexpr explicit GameRuleReadyEvent(GameRuleRegistry& registry) : mRegistry(registry) {}

    MOD_NDAPI GameRuleRegistry& registry() const;
};

struct GameRuleRegistration {
    std::string                    mIdentifier;
    std::variant<bool, int, float> mDefaultValue          = false;
    bool                           mRequiresCheats        = false;
    bool                           mShouldSaveToDisk      = true;
    bool                           mAllowUseInCommand     = true;
    bool                           mAllowUseInScripting   = true;
    bool                           mCanBeModifiedByPlayer = true;
};

class GameRuleRegistrationBuilder {
    GameRuleRegistration mRegistration;

public:
    explicit GameRuleRegistrationBuilder(std::string identifier) { mRegistration.mIdentifier = std::move(identifier); }

    GameRuleRegistrationBuilder& boolDefault(bool value) {
        mRegistration.mDefaultValue = value;
        return *this;
    }

    GameRuleRegistrationBuilder& intDefault(int value) {
        mRegistration.mDefaultValue = value;
        return *this;
    }

    GameRuleRegistrationBuilder& floatDefault(float value) {
        mRegistration.mDefaultValue = value;
        return *this;
    }

    GameRuleRegistrationBuilder& requiresCheats(bool value) {
        mRegistration.mRequiresCheats = value;
        return *this;
    }

    GameRuleRegistrationBuilder& shouldSaveToDisk(bool value) {
        mRegistration.mShouldSaveToDisk = value;
        return *this;
    }

    GameRuleRegistrationBuilder& allowUseInCommand(bool value) {
        mRegistration.mAllowUseInCommand = value;
        return *this;
    }

    GameRuleRegistrationBuilder& allowUseInScripting(bool value) {
        mRegistration.mAllowUseInScripting = value;
        return *this;
    }

    GameRuleRegistrationBuilder& canBeModifiedByPlayer(bool value) {
        mRegistration.mCanBeModifiedByPlayer = value;
        return *this;
    }

    [[nodiscard]] DataRegistration<GameRuleRegistration> build() const { return {mRegistration}; }
};

class GameRuleRegistry {
public:
    struct Impl;
    std::unique_ptr<Impl> pImpl;

public:
    using EventType  = GameRuleReadyEvent;
    using Product    = ::GameRule;
    using ProductRef = optional_ref<::GameRule>;
    using Property   = GameRuleRegistration;
    using DataEntry  = GameRuleRegistration;

public:
    GameRuleRegistry();
    GameRuleRegistry& operator=(GameRuleRegistry const&) = delete;
    GameRuleRegistry(GameRuleRegistry const&)            = delete;

public:
    MOD_NDAPI static GameRuleRegistry& getInstance();

    MOD_NDAPI bool isReady() const noexcept;

    MOD_API static void ensureEventRegistered();

    template <class Entry>
    static constexpr bool isEntry = std::same_as<Entry, GameRuleRegistration>;

    static GameRuleRegistrationBuilder builder(std::string identifier) {
        return GameRuleRegistrationBuilder{std::move(identifier)};
    }

    static GameRuleRegistration makeDataEntry(std::string identifier, GameRuleRegistration registration) {
        registration.mIdentifier = std::move(identifier);
        return registration;
    }

    ProductRef registerEntry(DataRegistration<GameRuleRegistration> registration) {
        return _appendRegistration(registration.mEntry);
    }

    MOD_NDAPI ProductRef _appendRegistration(GameRuleRegistration const& registration);

    MOD_NDAPI ProductRef _appendRule(::GameRule&& rule);

    MOD_API void _bindRules(::GameRules& rules);
};

} // namespace modapi::inline gamerule

static_assert(modapi::Registry<modapi::GameRuleRegistry>);
