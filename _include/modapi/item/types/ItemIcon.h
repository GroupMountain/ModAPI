#pragma once
#include <gmlib/Macros.h>
#include <iostream>
#include <unordered_map>

namespace gmlib::mod {

class ItemIcon {
public:
    std::unordered_map<std::string, std::string> mTextures;

public:
    MOD_NDAPI ItemIcon();
    MOD_NDAPI ItemIcon(const char* texture);
    MOD_NDAPI ItemIcon(std::string_view texture);

    MOD_API void add(std::string_view type, std::string_view texture);
};

} // namespace gmlib::mod
