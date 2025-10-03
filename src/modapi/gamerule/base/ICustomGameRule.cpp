#include "modapi/gamerule/base/ICustomGameRule.h"

namespace modapi::inline gamerule {

#define DEFINE_CUSTOM_GAME_RULE_IMPL(T)                                                                                \
    template <>                                                                                                        \
    ICustomGameRule<T>::~ICustomGameRule() = default;                                                                  \
    template <>                                                                                                        \
    bool ICustomGameRule<T>::requiresCheats() const {                                                                  \
        return false;                                                                                                  \
    }                                                                                                                  \
    template <>                                                                                                        \
    bool ICustomGameRule<T>::shouleSaveToDisk() const {                                                                \
        return true;                                                                                                   \
    }                                                                                                                  \
    template <>                                                                                                        \
    bool ICustomGameRule<T>::allowUseInCommand() const {                                                               \
        return true;                                                                                                   \
    }                                                                                                                  \
    template <>                                                                                                        \
    bool ICustomGameRule<T>::allowUseInScripting() const {                                                             \
        return true;                                                                                                   \
    }                                                                                                                  \
    template <>                                                                                                        \
    bool ICustomGameRule<T>::canBeModifiedByPlayer() const {                                                           \
        return true;                                                                                                   \
    }

DEFINE_CUSTOM_GAME_RULE_IMPL(bool);
DEFINE_CUSTOM_GAME_RULE_IMPL(int);
DEFINE_CUSTOM_GAME_RULE_IMPL(float);

#undef DEFINE_CUSTOM_GAME_RULE_IMPL

} // namespace modapi::inline gamerule