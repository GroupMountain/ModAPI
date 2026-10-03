#pragma once
#include "modapi/Macros.h"
#include "modapi/Registry.h"
#include "modapi/gamerule/base/ICustomGameRule.h"
#include <ll/api/event/Event.h>
#include <mc/deps/core/utility/optional_ref.h>
#include <mc/world/level/storage/GameRule.h>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

class GameRules;

namespace modapi::inline gamerule {

class GameRuleRegistry;

// Published every time vanilla fills a `GameRules` instance and ModAPI may add rules to it.
//
// `GameRules::_registerRules` is called from the `GameRules` constructor and vanilla refills every
// instance it constructs, so this event can be published more than once per process; a deferred
// registration is applied to the instance the event carries.
class GameRuleReadyEvent final : public ll::event::Event {
    GameRuleRegistry& mRegistry;

public:
    constexpr explicit GameRuleReadyEvent(GameRuleRegistry& registry) : mRegistry(registry) {}

    MOD_NDAPI GameRuleRegistry& registry() const;
};

class GameRuleRegistry {
public:
    struct Impl;
    std::unique_ptr<Impl> pImpl;

public:
    using EventType  = GameRuleReadyEvent;
    using Product    = ::GameRule;
    using ProductRef = optional_ref<::GameRule>;

public:
    GameRuleRegistry();
    GameRuleRegistry& operator=(GameRuleRegistry const&) = delete;
    GameRuleRegistry(GameRuleRegistry const&)            = delete;

public:
    MOD_NDAPI static GameRuleRegistry& getInstance();

    // True once a GameRules instance has been filled, i.e. once rules can be appended.
    MOD_NDAPI bool isReady() const noexcept;

    // Registers the ready event emitter (idempotent); called by `ModAPI::load()` and by every
    // `DeferredRegister<GameRuleRegistry, ...>`.
    MOD_API static void ensureEventRegistered();

    // Entry types are implementations of ICustomGameRule<bool>, ICustomGameRule<int> or
    // ICustomGameRule<float>.
    template <class Entry>
    static constexpr bool isEntry =
        std::is_base_of_v<ICustomGameRule<bool>, Entry> || std::is_base_of_v<ICustomGameRule<int>, Entry>
        || std::is_base_of_v<ICustomGameRule<float>, Entry>;

    // Builds `Entry` and appends its rule to the current GameRules instance.
    template <class Entry, class... Args>
        requires isEntry<Entry>
    ProductRef registerEntry(Args&&... args) {
        return registerGameRule<Entry>(std::forward<Args>(args)...);
    }

    template <class Entry, class... Args>
        requires isEntry<Entry>
    ProductRef registerGameRule(Args&&... args) {
        try {
            auto rule = std::make_shared<Entry>(std::forward<Args>(args)...);

            ::GameRule gameRule(rule->getIdentifier(), rule->canBeModifiedByPlayer());
            gameRule.mShouldSave          = rule->shouleSaveToDisk();
            gameRule.mType                = ruleTypeOf<Entry>();
            gameRule.mValue               = rule->getDefaultValue();
            gameRule.mAllowUseInCommand   = rule->allowUseInCommand();
            gameRule.mAllowUseInScripting = rule->allowUseInScripting();
            gameRule.mIsDefaultSet        = true;
            gameRule.mRequiresCheats      = rule->requiresCheats();

            return _appendRule(std::move(gameRule));
        } catch (...) {
            return {};
        }
    }

    MOD_NDAPI ProductRef registerGameRuleBool(
        std::string const& identifier,
        bool               defaultValue,
        bool               requiresCheats        = false,
        bool               shouleSaveToDisk      = true,
        bool               allowUseInCommand     = true,
        bool               allowUseInScripting   = true,
        bool               canBeModifiedByPlayer = true
    );

    MOD_NDAPI ProductRef registerGameRuleInt(
        std::string const& identifier,
        int                defaultValue,
        bool               requiresCheats        = false,
        bool               shouleSaveToDisk      = true,
        bool               allowUseInCommand     = true,
        bool               allowUseInScripting   = true,
        bool               canBeModifiedByPlayer = true
    );

    MOD_NDAPI ProductRef registerGameRuleFloat(
        std::string const& identifier,
        float              defaultValue,
        bool               requiresCheats        = false,
        bool               shouleSaveToDisk      = true,
        bool               allowUseInCommand     = true,
        bool               allowUseInScripting   = true,
        bool               canBeModifiedByPlayer = true
    );

    // Appends a fully built rule to the current GameRules instance; empty when the registry has not
    // opened yet.
    MOD_NDAPI ProductRef _appendRule(::GameRule&& rule);

    // Binds the instance vanilla is filling and publishes `GameRuleReadyEvent` for it.
    MOD_API void _bindRules(::GameRules& rules);

private:
    template <class Entry>
    static constexpr ::GameRule::Type ruleTypeOf() {
        if constexpr (std::is_base_of_v<ICustomGameRule<bool>, Entry>) {
            return ::GameRule::Type::Bool;
        } else if constexpr (std::is_base_of_v<ICustomGameRule<int>, Entry>) {
            return ::GameRule::Type::Int;
        } else {
            return ::GameRule::Type::Float;
        }
    }
};

} // namespace modapi::inline gamerule

static_assert(modapi::Registry<modapi::GameRuleRegistry>);
