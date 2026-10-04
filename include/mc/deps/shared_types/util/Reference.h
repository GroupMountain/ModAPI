#pragma once

#include <mc/_HeaderOutputPredefine.h>
#include <string>

// Mirror of the engine's `mc/deps/shared_types/util/Reference.h`. It is kept here so the template is visible from the
// server headers this project builds against; the include order puts this directory before the LeviLamina package, the
// same way the other patched engine headers in this tree work.
namespace SharedTypes {

template <int type> // SharedTypes::AssetType
struct Reference {
    std::string mValue;

    bool     operator==(Reference const& other) const { return mValue == other.mValue; }
    explicit operator std::string const&() const { return mValue; }
};

} // namespace SharedTypes
