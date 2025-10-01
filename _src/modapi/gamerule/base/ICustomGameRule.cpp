#include <gmlib/mod/gamerule/base/ICustomGameRule.h>

namespace gmlib::mod {

template <>
ICustomGameRule<bool>::~ICustomGameRule() = default;
template <>
ICustomGameRule<int>::~ICustomGameRule() = default;
template <>
ICustomGameRule<float>::~ICustomGameRule() = default;

template <>
bool ICustomGameRule<bool>::requiresCheats() const {
    return false;
}
template <>
bool ICustomGameRule<int>::requiresCheats() const {
    return false;
}
template <>
bool ICustomGameRule<float>::requiresCheats() const {
    return false;
}
template <>
bool ICustomGameRule<bool>::shouleSaveToDisk() const {
    return true;
}
template <>
bool ICustomGameRule<int>::shouleSaveToDisk() const {
    return true;
}
template <>
bool ICustomGameRule<float>::shouleSaveToDisk() const {
    return true;
}

template <>
bool ICustomGameRule<bool>::allowUseInCommand() const {
    return true;
}
template <>
bool ICustomGameRule<int>::allowUseInCommand() const {
    return true;
}
template <>
bool ICustomGameRule<float>::allowUseInCommand() const {
    return true;
}

template <>
bool ICustomGameRule<bool>::allowUseInScripting() const {
    return true;
}
template <>
bool ICustomGameRule<int>::allowUseInScripting() const {
    return true;
}
template <>
bool ICustomGameRule<float>::allowUseInScripting() const {
    return true;
}

template <>
bool ICustomGameRule<bool>::canBeModifiedByPlayer() const {
    return true;
}
template <>
bool ICustomGameRule<int>::canBeModifiedByPlayer() const {
    return true;
}
template <>
bool ICustomGameRule<float>::canBeModifiedByPlayer() const {
    return true;
}

} // namespace gmlib::mod