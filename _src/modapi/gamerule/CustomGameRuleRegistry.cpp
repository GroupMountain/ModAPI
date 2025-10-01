#include <gmlib/mod/gamerule/CustomGameRuleRegistry.h>
#include <ll/api/memory/Hook.h>
#include <mc/world/level/storage/GameRule.h>
#include <mc/world/level/storage/GameRuleId.h>
#include <mc/world/level/storage/GameRuleUtils.h>
#include <mc/world/level/storage/GameRules.h>

namespace gmlib::mod {

struct RegisterRulesHook;

struct CustomGameRuleRegistry::Impl {
    std::vector<std::function<std::unique_ptr<ICustomGameRule<bool>>()>>  mPendingGameRulesBool;
    std::vector<std::function<std::unique_ptr<ICustomGameRule<int>>()>>   mPendingGameRulesInt;
    std::vector<std::function<std::unique_ptr<ICustomGameRule<float>>()>> mPendingGameRulesFloat;
    ll::memory::HookRegistrar<RegisterRulesHook>                          mHook;
};

#define REGISTER_EDU_GAMERULE(ruleName, defaultValue)                                                                  \
    try {                                                                                                              \
        auto rule                          = GameRule(ruleName, true);                                                 \
        rule.mShouldSave                   = true;                                                                     \
        rule.mType                         = ::GameRule::Type::Bool;                                                   \
        rule.mValue->boolVal = defaultValue;                                                             \
        rule.mAllowUseInCommand            = true;                                                                     \
        rule.mAllowUseInScripting          = true;                                                                     \
        rule.mIsDefaultSet                 = true;                                                                     \
        rule.mRequiresCheats               = false;                                                                    \
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
            auto customRule  = func();
            auto rule        = GameRule(customRule->getIdentifier(), customRule->canBeModifiedByPlayer());
            rule.mShouldSave = customRule->shouleSaveToDisk();
            rule.mType       = ::GameRule::Type::Bool;
            rule.mValue->boolVal = customRule->getDefaultValue();
            rule.mAllowUseInCommand            = customRule->allowUseInCommand();
            rule.mAllowUseInScripting          = customRule->allowUseInScripting();
            rule.mIsDefaultSet                 = true;
            rule.mRequiresCheats               = customRule->requiresCheats();
            mGameRules->push_back(std::move(rule));
        } catch (...) {}
    }
    for (auto& func : registry.mPendingGameRulesInt) {
        try {
            auto customRule  = func();
            auto rule        = GameRule(customRule->getIdentifier(), customRule->canBeModifiedByPlayer());
            rule.mShouldSave = customRule->shouleSaveToDisk();
            rule.mType       = ::GameRule::Type::Int;
            rule.mValue->intVal = customRule->getDefaultValue();
            rule.mAllowUseInCommand           = customRule->allowUseInCommand();
            rule.mAllowUseInScripting         = customRule->allowUseInScripting();
            rule.mIsDefaultSet                = true;
            rule.mRequiresCheats              = customRule->requiresCheats();
            mGameRules->push_back(std::move(rule));
        } catch (...) {}
    }
    for (auto& func : registry.mPendingGameRulesFloat) {
        try {
            auto customRule  = func();
            auto rule        = GameRule(customRule->getIdentifier(), customRule->canBeModifiedByPlayer());
            rule.mShouldSave = customRule->shouleSaveToDisk();
            rule.mType       = ::GameRule::Type::Float;
            rule.mValue->floatVal = customRule->getDefaultValue();
            rule.mAllowUseInCommand             = customRule->allowUseInCommand();
            rule.mAllowUseInScripting           = customRule->allowUseInScripting();
            rule.mIsDefaultSet                  = true;
            rule.mRequiresCheats                = customRule->requiresCheats();
            mGameRules->push_back(std::move(rule));
        } catch (...) {}
    }
}

CustomGameRuleRegistry::CustomGameRuleRegistry() : pImpl(std::make_unique<Impl>()) {}

CustomGameRuleRegistry& CustomGameRuleRegistry::getInstance() {
    static CustomGameRuleRegistry instance;
    return instance;
}

CustomGameRuleRegistry&
CustomGameRuleRegistry::_registerGameRule(std::function<std::unique_ptr<ICustomGameRule<bool>>()>&& func) {
    pImpl->mPendingGameRulesBool.push_back(std::move(func));
    return *this;
}

CustomGameRuleRegistry&
CustomGameRuleRegistry::_registerGameRule(std::function<std::unique_ptr<ICustomGameRule<int>>()>&& func) {
    pImpl->mPendingGameRulesInt.push_back(std::move(func));
    return *this;
}

CustomGameRuleRegistry&
CustomGameRuleRegistry::_registerGameRule(std::function<std::unique_ptr<ICustomGameRule<float>>()>&& func) {
    pImpl->mPendingGameRulesFloat.push_back(std::move(func));
    return *this;
}

class CustomGameRuleBoolBase : public ICustomGameRule<bool> {
    std::string mIdentifier;
    bool        mDefaultValue;
    bool        mRequiresCheats;
    bool        mShouleSaveToDisk;
    bool        mAllowUseInCommand;
    bool        mAllowUseInScripting;
    bool        mCanBeModifiedByPlayer;

public:
    CustomGameRuleBoolBase(
        std::string const& identifier,
        bool               defaultValue,
        bool               requiresCheats,
        bool               shouleSaveToDisk,
        bool               allowUseInCommand,
        bool               allowUseInScripting,
        bool               canBeModifiedByPlayer
    )
    : ICustomGameRule<bool>(),
      mIdentifier(identifier),
      mDefaultValue(defaultValue),
      mRequiresCheats(requiresCheats),
      mShouleSaveToDisk(shouleSaveToDisk),
      mAllowUseInCommand(allowUseInCommand),
      mAllowUseInScripting(allowUseInScripting),
      mCanBeModifiedByPlayer(canBeModifiedByPlayer) {}

    std::string getIdentifier() const override { return mIdentifier; }

    bool getDefaultValue() const override { return mDefaultValue; }

    bool requiresCheats() const override { return mRequiresCheats; }

    bool shouleSaveToDisk() const override { return mShouleSaveToDisk; }

    bool allowUseInCommand() const override { return mAllowUseInCommand; }

    bool allowUseInScripting() const override { return mAllowUseInScripting; }

    bool canBeModifiedByPlayer() const override { return mCanBeModifiedByPlayer; }
};

CustomGameRuleRegistry& CustomGameRuleRegistry::registerGameRuleBool(
    std::string const& identifier,
    bool               defaultValue,
    bool               requiresCheats,
    bool               shouleSaveToDisk,
    bool               allowUseInCommand,
    bool               allowUseInScripting,
    bool               canBeModifiedByPlayer
) {
    return registerGameRule<CustomGameRuleBoolBase>(
        identifier,
        defaultValue,
        requiresCheats,
        shouleSaveToDisk,
        allowUseInCommand,
        allowUseInScripting,
        canBeModifiedByPlayer
    );
}

class CustomGameRuleIntBase : public ICustomGameRule<int> {
    std::string mIdentifier;
    int         mDefaultValue;
    bool        mRequiresCheats;
    bool        mShouleSaveToDisk;
    bool        mAllowUseInCommand;
    bool        mAllowUseInScripting;
    bool        mCanBeModifiedByPlayer;

public:
    CustomGameRuleIntBase(
        std::string const& identifier,
        int                defaultValue,
        bool               requiresCheats,
        bool               shouleSaveToDisk,
        bool               allowUseInCommand,
        bool               allowUseInScripting,
        bool               canBeModifiedByPlayer
    )
    : ICustomGameRule<int>(),
      mIdentifier(identifier),
      mDefaultValue(defaultValue),
      mRequiresCheats(requiresCheats),
      mShouleSaveToDisk(shouleSaveToDisk),
      mAllowUseInCommand(allowUseInCommand),
      mAllowUseInScripting(allowUseInScripting),
      mCanBeModifiedByPlayer(canBeModifiedByPlayer) {}

    std::string getIdentifier() const override { return mIdentifier; }

    int getDefaultValue() const override { return mDefaultValue; }

    bool requiresCheats() const override { return mRequiresCheats; }

    bool shouleSaveToDisk() const override { return mShouleSaveToDisk; }

    bool allowUseInCommand() const override { return mAllowUseInCommand; }

    bool allowUseInScripting() const override { return mAllowUseInScripting; }

    bool canBeModifiedByPlayer() const override { return mCanBeModifiedByPlayer; }
};

CustomGameRuleRegistry& CustomGameRuleRegistry::registerGameRuleInt(
    std::string const& identifier,
    int                defaultValue,
    bool               requiresCheats,
    bool               shouleSaveToDisk,
    bool               allowUseInCommand,
    bool               allowUseInScripting,
    bool               canBeModifiedByPlayer
) {
    return registerGameRule<CustomGameRuleIntBase>(
        identifier,
        defaultValue,
        requiresCheats,
        shouleSaveToDisk,
        allowUseInCommand,
        allowUseInScripting,
        canBeModifiedByPlayer
    );
}

class CustomGameRuleFloatBase : public ICustomGameRule<float> {
    std::string mIdentifier;
    float       mDefaultValue;
    bool        mRequiresCheats;
    bool        mShouleSaveToDisk;
    bool        mAllowUseInCommand;
    bool        mAllowUseInScripting;
    bool        mCanBeModifiedByPlayer;

public:
    CustomGameRuleFloatBase(
        std::string const& identifier,
        float              defaultValue,
        bool               requiresCheats,
        bool               shouleSaveToDisk,
        bool               allowUseInCommand,
        bool               allowUseInScripting,
        bool               canBeModifiedByPlayer
    )
    : ICustomGameRule<float>(),
      mIdentifier(identifier),
      mDefaultValue(defaultValue),
      mRequiresCheats(requiresCheats),
      mShouleSaveToDisk(shouleSaveToDisk),
      mAllowUseInCommand(allowUseInCommand),
      mAllowUseInScripting(allowUseInScripting),
      mCanBeModifiedByPlayer(canBeModifiedByPlayer) {}

    std::string getIdentifier() const override { return mIdentifier; }

    float getDefaultValue() const override { return mDefaultValue; }

    bool requiresCheats() const override { return mRequiresCheats; }

    bool shouleSaveToDisk() const override { return mShouleSaveToDisk; }

    bool allowUseInCommand() const override { return mAllowUseInCommand; }

    bool allowUseInScripting() const override { return mAllowUseInScripting; }

    bool canBeModifiedByPlayer() const override { return mCanBeModifiedByPlayer; }
};

CustomGameRuleRegistry& CustomGameRuleRegistry::registerGameRuleFloat(
    std::string const& identifier,
    float              defaultValue,
    bool               requiresCheats,
    bool               shouleSaveToDisk,
    bool               allowUseInCommand,
    bool               allowUseInScripting,
    bool               canBeModifiedByPlayer
) {
    return registerGameRule<CustomGameRuleFloatBase>(
        identifier,
        defaultValue,
        requiresCheats,
        shouleSaveToDisk,
        allowUseInCommand,
        allowUseInScripting,
        canBeModifiedByPlayer
    );
}

} // namespace gmlib::mod