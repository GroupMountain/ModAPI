#include "modapi/item/types/ItemIcon.h"

namespace modapi::inline base {

ItemIcon::ItemIcon() = default;

ItemIcon::ItemIcon(const char* texture) { mTextures["default"] = texture; }

ItemIcon::ItemIcon(std::string_view texture) { mTextures["default"] = texture; }

void ItemIcon::add(std::string_view type, std::string_view texture) { mTextures.emplace(type, texture); }

} // namespace modapi::inline base