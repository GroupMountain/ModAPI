#pragma once
#include "modapi/item/base/ICustomArmorItem.h"
#include "modapi/item/base/ICustomBlockItem.h"
#include "modapi/item/base/ICustomItem.h"
#include "modapi/item/shared_types/FoodItemComponentLegacy.h"
#include <ll/api/utils/StringUtils.h>
#include <magic_enum/magic_enum.hpp>
#include <mc/deps/nbt/CompoundTag.h>
#include <mc/world/item/ItemTag.h>
#include <mc/world/item/ResolvedItemIconInfo.h>

namespace modapi::inline item {

// 26.51 replaced `Item`'s `bool mAllowOffhand` bit with the tri-state
// `Item::OffhandAllowed`; these two keep the old boolean view of the field.
MOD_API ::Item::OffhandAllowed toOffhandAllowed(bool allowed);

MOD_API bool isOffhandAllowed(::Item::OffhandAllowed value);

// 此处不要自作聪明改成 enum_name 转 snake_case
MOD_API std::string buildEnchantSlot(::Enchant::Slot slot);

MOD_API std::string buildArmorSlot(::SharedTypes::Legacy::ArmorSlot slot);

std::unique_ptr<::CompoundTag> buildClientComponents(ICustomArmorItem const& item);

template <class ItemT>
std::unique_ptr<::CompoundTag> buildClientComponents(ItemT const& item) {
    auto  result         = std::make_unique<::CompoundTag>();
    auto& builder        = *result;
    builder["item_tags"] = ::ListTag();
    for (auto& tag : *item.mTags) {
        builder["item_tags"].push_back(tag.getString());
    }
    builder["item_properties"]            = ::CompoundTag();
    auto& properties                      = builder["item_properties"];
    properties["allow_off_hand"]          = isOffhandAllowed(item.mAllowOffhand);
    properties["can_destroy_in_creative"] = item.canDestroyInCreative();
    properties["creative_category"]       = (int)item.mCreativeCategory;
    properties["creative_group"]          = *item.mCreativeGroup;
    properties["damage"]                  = item.getAttackDamage();
    properties["enchantable_slot"]        = buildEnchantSlot(::Enchant::Slot(item.getEnchantSlot()));
    properties["enchantable_value"]       = item.getEnchantValue();
    properties["foil"]                    = item.mIsGlint;
    properties["hand_equipped"]           = item.mHandEquipped;
    properties["liquid_clipped"]          = item.isLiquidClipItem();
    properties["max_stack_size"]          = (int)item.mMaxStackSize;
    auto iconInfo                         = item.getIcon().mTextures;
    if (!iconInfo.empty()) {
        for (auto& [key, val] : iconInfo) {
            properties["minecraft:icon"]["textures"][key] = val;
            // Top level as well: that is where a client reads an item's icon from, and writing it only under
            // `item_properties` is what made a plain custom item come up as a blank square with
            // "requires either an icon atlas or icon texture", while the block item - which writes both -
            // showed its texture.
            builder["minecraft:icon"]["textures"][key] = val;
        }
        properties["minecraft:icon"]["texture"] = iconInfo.begin()->second;
        builder["minecraft:icon"]["texture"]    = iconInfo.begin()->second;
    }
    properties["mining_speed"]    = item.getMiningSpeed();
    properties["should_despawn"]  = item.mShouldDespawn;
    properties["stacked_by_data"] = item.mIsStackedByData;
    properties["use_animation"]   = (int)item.mUseAnim;
    properties["use_duration"]    = item.mMaxUseDuration;
    if (!item.getDisplayName().empty()) {
        builder["minecraft:display_name"]["value"] = item.getDisplayName();
    }
    builder["minecraft:rarity"]["value"] = ll::string_utils::toSnakeCase(magic_enum::enum_name(item.getBaseRarity()));
    if (item.getMaxDamage() > 0) {
        builder["minecraft:durability"]["max_durability"]       = (int)item.getMaxDamage();
        builder["minecraft:durability"]["damage_chance"]["min"] = (int)item.getItemDamageChance().mMin;
        builder["minecraft:durability"]["damage_chance"]["max"] = (int)item.getItemDamageChance().mMax;
    }
    if (item.mFurnaceBurnIntervalModifier > 0) {
        builder["minecraft:fuel"]["duration"] = item.mFurnaceBurnIntervalModifier;
    }
    if (item.mMaxDamage > 0 && !item.getRepairItems().empty()) {
        builder["minecraft:repairable"]["repair_items"] = ::ListTag();
        for (auto& itemsInfo : item.getRepairItems()) {
            auto data = CompoundTag({
                {"items",         ListTag()                                                  },
                {"repair_amount", (float)(0.25 * item.mMaxDamage) /*itemsInfo.mRepairAmount*/}  // TODO: fix this
            });
            for (auto& repairItem : itemsInfo.mItems) {
                data["items"].push_back({
                    {"name", repairItem}
                });
            }
            builder["minecraft:repairable"]["repair_items"].push_back(std::move(data));
        }
    }
    if (item.isThrowable()) {
        builder["minecraft:projectile"] = {
            {"minimum_critical_power", 0.0f},
            {"projectile_entity",      ""  }
        };
        builder["minecraft:throwable"] = {
            {"do_swing_animation",           true },
            {"launch_power_scale",           0.0f },
            {"max_draw_duration",            0.0f },
            {"max_launch_power",             0.0f },
            {"min_draw_duration",            0.0f },
            {"scale_power_by_draw_duration", false}
        };
    }
    if (item.getCooldownDuration() > 0 && !item.getCooldownCategory().getString().empty()) {
        builder["minecraft:cooldown"]["category"]               = item.getCooldownCategory().getString();
        builder["minecraft:cooldown"]["duration"]               = (float)item.getCooldownDuration() / 20.0f;
        builder["minecraft:use_modifiers"]["movement_modifier"] = 0.35f;
        builder["minecraft:use_modifiers"]["use_duration"]      = (float)item.mMaxUseDuration / 20.0f;
    }
    if (item.requiresInteract()) {
        builder["minecraft:interact_button"]["requires_interact"] = true;
        builder["minecraft:interact_button"]["interact_text"]     = item.getInteractButtonText();
    }
    if (item.isFood() && item.mFoodComponentLegacy) {
        builder["minecraft:food"]                               = *item.mFoodComponentLegacy->buildNetworkTag();
        builder["minecraft:use_modifiers"]["movement_modifier"] = 0.35f;
        builder["minecraft:use_modifiers"]["use_duration"]      = (float)item.mMaxUseDuration / 20.0f;
    }
    return std::move(result);
}

// A block item has no item document of its own: its icon and placement come from the block, so only the
// properties a client needs for the item are built here.
std::unique_ptr<::CompoundTag> buildClientComponents(ICustomBlockItem const& item);

} // namespace modapi::inline item
