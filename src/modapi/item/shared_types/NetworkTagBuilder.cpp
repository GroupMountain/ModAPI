#include "modapi/item/shared_types/NetworkTagBuilder.h"
#include "modapi/item/shared_types/FoodItemComponentLegacy.h"

namespace modapi::inline item {

// 此处不要自作聪明改成 enum_name 转 snake_case
std::string buildEnchantSlot(::Enchant::Slot slot) {
    switch (slot) {
    case ::Enchant::Slot::ArmorHead:
        return "armor_head";
    case ::Enchant::Slot::ArmorTorso:
        return "armor_torso";
    case ::Enchant::Slot::ArmorFeet:
        return "armor_feet";
    case ::Enchant::Slot::ArmorLegs:
        return "armor_legs";
    case ::Enchant::Slot::Sword:
        return "sword";
    case ::Enchant::Slot::Bow:
        return "bow";
    case ::Enchant::Slot::Hoe:
        return "hoe";
    case ::Enchant::Slot::Shears:
        return "shears";
    case ::Enchant::Slot::Flintsteel:
        return "flintsteel";
    case ::Enchant::Slot::Axe:
        return "axe";
    case ::Enchant::Slot::Pickaxe:
        return "pickaxe";
    case ::Enchant::Slot::Shovel:
        return "shovel";
    case ::Enchant::Slot::FishingRod:
        return "fishing_rod";
    case ::Enchant::Slot::CarrotStick:
        return "carrot_stick";
    case ::Enchant::Slot::Elytra:
        return "elytra";
    case ::Enchant::Slot::Spear:
        return "spear";
    case ::Enchant::Slot::Crossbow:
        return "crossbow";
    case ::Enchant::Slot::Shield:
        return "shield";
    case ::Enchant::Slot::CosmeticHead:
        return "cosmetic_head";
    case ::Enchant::Slot::Compass:
        return "compass";
    case ::Enchant::Slot::MushroomStick:
        return "mushroom_stick";
    case ::Enchant::Slot::Brush:
        return "brush";
    case ::Enchant::Slot::HeavyWeapon:
        return "heavy_weapon";
    case ::Enchant::Slot::GArmor:
        return "garmor"; // 是否正确？？
    case ::Enchant::Slot::GDigging:
        return "gdigging"; // 是否正确？？
    case ::Enchant::Slot::GTool:
        return "gtool"; // 是否正确？？
    case ::Enchant::Slot::All:
        return "all"; // 是否正确？？
    default:
        return "none";
    }
}

std::string buildArmorSlot(::SharedTypes::Legacy::ArmorSlot slot) {
    switch (slot) {
    case ::SharedTypes::Legacy::ArmorSlot::Head:
        return "slot.armor.head";
    case ::SharedTypes::Legacy::ArmorSlot::Torso:
        return "slot.armor.chest";
    case ::SharedTypes::Legacy::ArmorSlot::Legs:
        return "slot.armor.legs";
    case ::SharedTypes::Legacy::ArmorSlot::Feet:
        return "slot.armor.feet";
    default:
        return {};
    }
}

std::unique_ptr<::CompoundTag> buildClientComponents(ICustomArmorItem const& item) {
    auto  result         = std::make_unique<::CompoundTag>();
    auto& builder        = *result;
    builder["item_tags"] = ::ListTag();
    for (auto& tag : *item.mTags) {
        builder["item_tags"].push_back(tag.getString());
    }
    builder["item_properties"]            = ::CompoundTag();
    auto& properties                      = builder["item_properties"];
    properties["allow_off_hand"]          = item.mAllowOffhand;
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
        }
    }
    properties["mining_speed"]    = item.getMiningSpeed();
    properties["should_despawn"]  = item.mShouldDespawn;
    properties["stacked_by_data"] = item.mIsStackedByData;
    properties["use_animation"]   = (int)item.mUseAnim;
    properties["use_duration"]    = item.mMaxUseDuration;
    if (!item.getDisplayName().empty()) {
        builder["minecraft:display_name"]["value"] = item.getDisplayName();
    }
    if (item.isHumanoidArmor()) {
        auto slot = buildArmorSlot(item.getArmorSlot());
        if (!slot.empty()) {
            builder["minecraft:wearable"]["slot"]       = slot;
            builder["minecraft:wearable"]["protection"] = item.getArmorValue();
        }
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
    if (item.getCooldownTime() > 0 && !item.getCooldownType().getString().empty()) {
        builder["minecraft:cooldown"]["category"]               = item.getCooldownType().getString();
        builder["minecraft:cooldown"]["duration"]               = (float)item.getCooldownTime() / 20.0f;
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

std::unique_ptr<::CompoundTag> buildClientComponents(ICustomItem const& item) {
    auto  result         = std::make_unique<::CompoundTag>();
    auto& builder        = *result;
    builder["item_tags"] = ::ListTag();
    for (auto& tag : *item.mTags) {
        builder["item_tags"].push_back(tag.getString());
    }
    builder["item_properties"]            = ::CompoundTag();
    auto& properties                      = builder["item_properties"];
    properties["allow_off_hand"]          = item.mAllowOffhand;
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
        }
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
    if (item.getCooldownTime() > 0 && !item.getCooldownType().getString().empty()) {
        builder["minecraft:cooldown"]["category"]               = item.getCooldownType().getString();
        builder["minecraft:cooldown"]["duration"]               = (float)item.getCooldownTime() / 20.0f;
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

} // namespace modapi::inline item
