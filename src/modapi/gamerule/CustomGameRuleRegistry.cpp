#include "modapi/gamerule/CustomGameRuleRegistry.h"
#include <ll/api/memory/Hook.h>
#include <mc/world/level/storage/GameRule.h>
#include <mc/world/level/storage/GameRuleId.h>
#include <mc/world/level/storage/GameRuleUtils.h>
#include <mc/world/level/storage/GameRules.h>
#include <utility>

namespace modapi::inline gamerule {

struct RegisterRulesHook;

struct CustomGameRuleRegistry::Impl {
    ll::memory::HookRegistrar<RegisterRulesHook>                          mHook;
    std::vector<std::function<std::unique_ptr<ICustomGameRule<bool>>()>>  mPendingGameRulesBool;
    std::vector<std::function<std::unique_ptr<ICustomGameRule<int>>()>>   mPendingGameRulesInt;
    std::vector<std::function<std::unique_ptr<ICustomGameRule<float>>()>> mPendingGameRulesFloat;
};

#define REGISTER_EDU_GAMERULE(ruleName, defaultValue)                                                                  \
    try {                                                                                                              \
        auto rule                 = GameRule(ruleName, true);                                                          \
        rule.mShouldSave          = true;                                                                              \
        rule.mType                = ::GameRule::Type::Bool;                                                            \
        rule.mValue->boolVal      = defaultValue;                                                                      \
        rule.mAllowUseInCommand   = true;                                                                              \
        rule.mAllowUseInScripting = true;                                                                              \
        rule.mIsDefaultSet        = true;                                                                              \
        rule.mRequiresCheats      = false;                                                                             \
        mGameRules->push_back(std::move(rule));                                                                        \
    } catch (...) {}

LL_TYPE_INSTANCE_HOOK(RegisterRulesHook, HookPriority::Normal, GameRules, &GameRules::_registerRules, void) {
    origin();
    auto& registry = *CustomGameRuleRegistry::getInstance().pImpl;
    // Edu GameRules
    REGISTER_EDU_GAMERULE("globalmute", false)
    REGISTER_EDU_GAMERULE("allowdestructiveobjects", true)
    REGISTER_EDU_GAMERULE("allowmobs", true)
    REGISTER_EDU_GAMERULE("codebuilder", true)
    REGISTER_EDU_GAMERULE("educloudsave", false)
    // Custom GameRules
    for (auto& func : registry.mPendingGameRulesBool) {
        try {
            auto customRule           = func();
            auto rule                 = GameRule(customRule->getIdentifier(), customRule->canBeModifiedByPlayer());
            rule.mShouldSave          = customRule->shouleSaveToDisk();
            rule.mType                = ::GameRule::Type::Bool;
            rule.mValue->boolVal      = customRule->getDefaultValue();
            rule.mAllowUseInCommand   = customRule->allowUseInCommand();
            rule.mAllowUseInScripting = customRule->allowUseInScripting();
            rule.mIsDefaultSet        = true;
            rule.mRequiresCheats      = customRule->requiresCheats();
            mGameRules->push_back(std::move(rule));
        } catch (...) {}
    }
    for (auto& func : registry.mPendingGameRulesInt) {
        try {
            auto customRule           = func();
            auto rule                 = GameRule(customRule->getIdentifier(), customRule->canBeModifiedByPlayer());
            rule.mShouldSave          = customRule->shouleSaveToDisk();
            rule.mType                = ::GameRule::Type::Int;
            rule.mValue->intVal       = customRule->getDefaultValue();
            rule.mAllowUseInCommand   = customRule->allowUseInCommand();
            rule.mAllowUseInScripting = customRule->allowUseInScripting();
            rule.mIsDefaultSet        = true;
            rule.mRequiresCheats      = customRule->requiresCheats();
            mGameRules->push_back(std::move(rule));
        } catch (...) {}
    }
    for (auto& func : registry.mPendingGameRulesFloat) {
        try {
            auto customRule           = func();
            auto rule                 = GameRule(customRule->getIdentifier(), customRule->canBeModifiedByPlayer());
            rule.mShouldSave          = customRule->shouleSaveToDisk();
            rule.mType                = ::GameRule::Type::Float;
            rule.mValue->floatVal     = customRule->getDefaultValue();
            rule.mAllowUseInCommand   = customRule->allowUseInCommand();
            rule.mAllowUseInScripting = customRule->allowUseInScripting();
            rule.mIsDefaultSet        = true;
            rule.mRequiresCheats      = customRule->requiresCheats();
            mGameRules->push_back(std::move(rule));
        } catch (...) {}
    }
}

#undef REGISTER_EDU_GAMERULE

CustomGameRuleRegistry::CustomGameRuleRegistry() : pImpl(std::make_unique<Impl>()) {}

CustomGameRuleRegistry& CustomGameRuleRegistry::getInstance() {
    static CustomGameRuleRegistry instance;
    return instance;
}

#define CUSTOM_GAME_RULE_REGISTRY_IMPL(TYPE, TYPE_NAME)                                                                \
    class CustomGameRule##TYPE_NAME##Base : public ICustomGameRule<TYPE> {                                             \
    private:                                                                                                           \
        std::string mIdentifier;                                                                                       \
        TYPE        mDefaultValue;                                                                                     \
        bool        mRequiresCheats;                                                                                   \
        bool        mShouleSaveToDisk;                                                                                 \
        bool        mAllowUseInCommand;                                                                                \
        bool        mAllowUseInScripting;                                                                              \
        bool        mCanBeModifiedByPlayer;                                                                            \
                                                                                                                       \
    public:                                                                                                            \
        CustomGameRule##TYPE_NAME##Base(                                                                               \
            std::string identifier,                                                                                    \
            TYPE        defaultValue,                                                                                  \
            bool        requiresCheats,                                                                                \
            bool        shouleSaveToDisk,                                                                              \
            bool        allowUseInCommand,                                                                             \
            bool        allowUseInScripting,                                                                           \
            bool        canBeModifiedByPlayer                                                                          \
        )                                                                                                              \
        : ICustomGameRule<TYPE>(),                                                                                     \
          mIdentifier(std::move(identifier)),                                                                          \
          mDefaultValue(defaultValue),                                                                                 \
          mRequiresCheats(requiresCheats),                                                                             \
          mShouleSaveToDisk(shouleSaveToDisk),                                                                         \
          mAllowUseInCommand(allowUseInCommand),                                                                       \
          mAllowUseInScripting(allowUseInScripting),                                                                   \
          mCanBeModifiedByPlayer(canBeModifiedByPlayer) {}                                                             \
                                                                                                                       \
        std::string getIdentifier() const override { return mIdentifier; }                                             \
        TYPE        getDefaultValue() const override { return mDefaultValue; }                                         \
        bool        requiresCheats() const override { return mRequiresCheats; }                                        \
        bool        shouleSaveToDisk() const override { return mShouleSaveToDisk; }                                    \
        bool        allowUseInCommand() const override { return mAllowUseInCommand; }                                  \
        bool        allowUseInScripting() const override { return mAllowUseInScripting; }                              \
        bool        canBeModifiedByPlayer() const override { return mCanBeModifiedByPlayer; }                          \
    };                                                                                                                 \
                                                                                                                       \
    CustomGameRuleRegistry& CustomGameRuleRegistry::registerGameRule##TYPE_NAME(                                       \
        std::string const& identifier,                                                                                 \
        TYPE               defaultValue,                                                                               \
        bool               requiresCheats,                                                                             \
        bool               shouleSaveToDisk,                                                                           \
        bool               allowUseInCommand,                                                                          \
        bool               allowUseInScripting,                                                                        \
        bool               canBeModifiedByPlayer                                                                       \
    ) {                                                                                                                \
        return registerGameRule<CustomGameRule##TYPE_NAME##Base>(                                                      \
            identifier,                                                                                                \
            defaultValue,                                                                                              \
            requiresCheats,                                                                                            \
            shouleSaveToDisk,                                                                                          \
            allowUseInCommand,                                                                                         \
            allowUseInScripting,                                                                                       \
            canBeModifiedByPlayer                                                                                      \
        );                                                                                                             \
    }                                                                                                                  \
                                                                                                                       \
    CustomGameRuleRegistry& CustomGameRuleRegistry::_registerGameRule(                                                 \
        std::function<std::unique_ptr<ICustomGameRule<TYPE>>()>&& func                                                 \
    ) {                                                                                                                \
        pImpl->mPendingGameRules##TYPE_NAME.push_back(std::move(func));                                                \
        return *this;                                                                                                  \
    }

CUSTOM_GAME_RULE_REGISTRY_IMPL(bool, Bool);
CUSTOM_GAME_RULE_REGISTRY_IMPL(float, Float);
CUSTOM_GAME_RULE_REGISTRY_IMPL(int, Int);

#undef CUSTOM_GAME_RULE_REGISTRY_IMPL

} // namespace modapi::inline gamerule