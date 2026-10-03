#include "modapi/gamerule/GameRuleRegistry.h"
#include "modapi/core/RegistryEvent.h"
#include <atomic>
#include <ll/api/event/EventBus.h>
#include <ll/api/memory/Hook.h>
#include <ll/api/service/Bedrock.h>
#include <mc/world/level/Level.h>
#include <mc/world/level/storage/GameRule.h>
#include <mc/world/level/storage/GameRules.h>
#include <utility>

namespace modapi::inline gamerule {

struct RegisterRulesHook;

struct GameRuleRegistry::Impl {
    ll::memory::HookRegistrar<RegisterRulesHook> mHook;
    // The GameRules instance vanilla filled the last time the hook ran.
    ::GameRules* mCurrentRules = nullptr;

    // The instance a registration has to be appended to. The level's own GameRules wins: the hook
    // also fires for temporary instances (settings parsing, copies) which are already gone when a mod
    // registers afterwards, and appending to those is what used to crash.
    [[nodiscard]] ::GameRules* currentRules() const {
        if (auto level = ll::service::getLevel()) return &level->getGameRules();
        return mCurrentRules;
    }
};

// The education rules are only part of the client/education builds - the server build has no
// `GameRules::_registerEDURules` - so ModAPI keeps registering them here.
#define REGISTER_EDU_GAMERULE(ruleName, defaultValue)                                                                  \
    try {                                                                                                              \
        auto rule                 = GameRule(ruleName, true);                                                          \
        rule.mShouldSave          = true;                                                                              \
        rule.mType                = ::GameRule::Type::Bool;                                                            \
        rule.mValue               = defaultValue;                                                                      \
        rule.mAllowUseInCommand   = true;                                                                              \
        rule.mAllowUseInScripting = true;                                                                              \
        rule.mIsDefaultSet        = true;                                                                              \
        rule.mRequiresCheats      = false;                                                                             \
        mGameRules->push_back(std::move(rule));                                                                        \
    } catch (...) {}

LL_TYPE_INSTANCE_HOOK(RegisterRulesHook, HookPriority::Normal, GameRules, &GameRules::_registerRules, void) {
    origin();
    REGISTER_EDU_GAMERULE("globalmute", false)
    REGISTER_EDU_GAMERULE("allowdestructiveobjects", true)
    REGISTER_EDU_GAMERULE("allowmobs", true)
    REGISTER_EDU_GAMERULE("codebuilder", true)
    REGISTER_EDU_GAMERULE("educloudsave", false)
    // Vanilla refills the rule list on every construction, so the deferred registrations are
    // replayed for this instance through the ready event.
    GameRuleRegistry::getInstance()._bindRules(*this);
}

#undef REGISTER_EDU_GAMERULE

GameRuleRegistry::GameRuleRegistry() : pImpl(std::make_unique<Impl>()) {}

GameRuleRegistry& GameRuleRegistry::getInstance() {
    // Deliberately leaked: the hook registrar this instance owns must not unregister its hooks
    // while the process/DLL is being torn down.
    static GameRuleRegistry& instance = *new GameRuleRegistry();
    return instance;
}

bool GameRuleRegistry::isReady() const noexcept { return pImpl->currentRules() != nullptr; }

void GameRuleRegistry::ensureEventRegistered() {
    // The vanilla hooks live in the registry's implementation, so the singleton has to exist before
    // the pass that fires them; every consumer goes through this entry point.
    (void)getInstance();
    static std::atomic_bool registered = false;
    core::ensureRegistryEventRegistered<EventType>(registered);
}

GameRuleRegistry::ProductRef GameRuleRegistry::_appendRule(::GameRule&& rule) {
    auto* rules = pImpl->currentRules();
    if (rules == nullptr) {
        core::getLogger().error(
            "GameRuleRegistry: a game rule was registered before the game rules were filled; "
            "use DeferredRegister<GameRuleRegistry, ...> to register rules before the server starts."
        );
        return {};
    }
    try {
        rules->mGameRules->push_back(std::move(rule));
        return &rules->mGameRules->back();
    } catch (...) {
        return {};
    }
}

void GameRuleRegistry::_bindRules(::GameRules& rules) {
    pImpl->mCurrentRules = &rules;
    ll::event::EventBus::getInstance().publish(GameRuleReadyEvent{*this});
}

GameRuleRegistry::ProductRef GameRuleRegistry::registerGameRuleBool(
    std::string const& identifier,
    bool               defaultValue,
    bool               requiresCheats,
    bool               shouleSaveToDisk,
    bool               allowUseInCommand,
    bool               allowUseInScripting,
    bool               canBeModifiedByPlayer
) {
    try {
        ::GameRule rule(identifier, canBeModifiedByPlayer);
        rule.mShouldSave          = shouleSaveToDisk;
        rule.mType                = ::GameRule::Type::Bool;
        rule.mValue               = defaultValue;
        rule.mAllowUseInCommand   = allowUseInCommand;
        rule.mAllowUseInScripting = allowUseInScripting;
        rule.mIsDefaultSet        = true;
        rule.mRequiresCheats      = requiresCheats;
        return _appendRule(std::move(rule));
    } catch (...) {
        return {};
    }
}

GameRuleRegistry::ProductRef GameRuleRegistry::registerGameRuleInt(
    std::string const& identifier,
    int                defaultValue,
    bool               requiresCheats,
    bool               shouleSaveToDisk,
    bool               allowUseInCommand,
    bool               allowUseInScripting,
    bool               canBeModifiedByPlayer
) {
    try {
        ::GameRule rule(identifier, canBeModifiedByPlayer);
        rule.mShouldSave          = shouleSaveToDisk;
        rule.mType                = ::GameRule::Type::Int;
        rule.mValue               = defaultValue;
        rule.mAllowUseInCommand   = allowUseInCommand;
        rule.mAllowUseInScripting = allowUseInScripting;
        rule.mIsDefaultSet        = true;
        rule.mRequiresCheats      = requiresCheats;
        return _appendRule(std::move(rule));
    } catch (...) {
        return {};
    }
}

GameRuleRegistry::ProductRef GameRuleRegistry::registerGameRuleFloat(
    std::string const& identifier,
    float              defaultValue,
    bool               requiresCheats,
    bool               shouleSaveToDisk,
    bool               allowUseInCommand,
    bool               allowUseInScripting,
    bool               canBeModifiedByPlayer
) {
    try {
        ::GameRule rule(identifier, canBeModifiedByPlayer);
        rule.mShouldSave          = shouleSaveToDisk;
        rule.mType                = ::GameRule::Type::Float;
        rule.mValue               = defaultValue;
        rule.mAllowUseInCommand   = allowUseInCommand;
        rule.mAllowUseInScripting = allowUseInScripting;
        rule.mIsDefaultSet        = true;
        rule.mRequiresCheats      = requiresCheats;
        return _appendRule(std::move(rule));
    } catch (...) {
        return {};
    }
}

GameRuleRegistry& GameRuleReadyEvent::registry() const { return mRegistry; }

} // namespace modapi::inline gamerule
