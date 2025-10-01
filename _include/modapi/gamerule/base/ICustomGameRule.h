#pragma once
#include <gmlib/Macros.h>
#include <iostream>
#include <type_traits>

namespace gmlib::mod {

template <typename T>
class ICustomGameRule {
public:
    static_assert(
        std::is_same_v<T, bool> || std::is_same_v<T, int> || std::is_same_v<T, float>,
        "ICustomGameRule<T> can only be instantiated with T = bool, T = int or T = float"
    );

public:
    MOD_API virtual ~ICustomGameRule();

    virtual std::string getIdentifier() const = 0;

    virtual T getDefaultValue() const = 0;

    MODAPI_NDAPI virtual bool requiresCheats() const;

    MODAPI_NDAPI virtual bool shouleSaveToDisk() const;

    MODAPI_NDAPI virtual bool allowUseInCommand() const;

    MODAPI_NDAPI virtual bool allowUseInScripting() const;

    MODAPI_NDAPI virtual bool canBeModifiedByPlayer() const;
};

} // namespace gmlib::mod
