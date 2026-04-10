#pragma once
#include <string>
namespace SharedTypes {
template <int type>
struct Reference {
    std::string mValue;
    bool        operator==(Reference const& other) const { return mValue == other.mValue; }
    explicit    operator std::string const&() const { return mValue; }
};
} // namespace SharedTypes