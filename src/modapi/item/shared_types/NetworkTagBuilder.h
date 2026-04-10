#pragma once
#include "modapi/item/base/ICustomArmorItem.h"
#include "modapi/item/base/ICustomItem.h"
#include <ll/api/utils/StringUtils.h>
#include <magic_enum/magic_enum.hpp>
#include <mc/deps/nbt/CompoundTag.h>
#include <mc/world/item/ItemTag.h>

namespace modapi::inline item {

// 此处不要自作聪明改成 enum_name 转 snake_case
std::string buildEnchantSlot(::Enchant::Slot slot);

std::string buildArmorSlot(::SharedTypes::Legacy::ArmorSlot slot);

std::unique_ptr<::CompoundTag> buildClientComponents(ICustomArmorItem const& item);

std::unique_ptr<::CompoundTag> buildClientComponents(ICustomItem const& item);

} // namespace modapi::inline item
