#pragma include_alias("mc/deps/shared_types/util/Reference.h", "modapi/item/ReferencePatched.h")
#include "modapi/item/CustomItemRegistry.h"
#include "modapi/item/CustomCreativeItemRegistry.h"
#include "modapi/item/shared_types/NetworkTagBuilder.h"
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/event/server/ServerStartedEvent.h>
#include <ll/api/memory/Hook.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/utils/ErrorUtils.h>
#include <mc/common/SharedPtr.h>
#include <mc/deps/vanilla_components/ActorDataDirtyFlagsComponent.h>
#include <mc/deps/vanilla_components/ActorDataFlagComponent.h>
#include <mc/network/packet/ItemData.h>
#include <mc/network/packet/ItemRegistryPacket.h>
#include <mc/server/commands/CommandItem.h>
#include <mc/world/actor/item/ItemActor.h>
#include <mc/world/actor/provider/SynchedActorDataAccess.h>
#include <mc/world/item/ItemTag.h>
#include <mc/world/item/VanillaItems.h>
#include <mc/world/item/components/IconItemComponent.h>
#include <mc/world/item/registry/ItemRegistry.h>
#include <mc/world/item/registry/ItemRegistryRef.h>
#include <mc/world/level/Level.h>
#include <mc/world/level/Spawner.h>

namespace std {

template <>
class hash<ItemTag> {
public:
    size_t operator()(ItemTag const& tag) const { return tag.mStrHash; }
};

} // namespace std

bool operator==(ItemTag const& lhs, ItemTag const& rhs) { return lhs.mStr == rhs.mStr; }

namespace modapi::inline item {

struct RegisterItemsHook;
struct ClientItemActorFixHook;
struct VanillaItemDefinitionSendHook;

struct CustomItemRegistry::Impl {
    ::ItemRegistry*                                       mRegistry;
    std::vector<std::function<std::unique_ptr<Item>()>>   mPendingItems;
    std::vector<std::function<void(CustomItemRegistry&)>> mPendingModifyItems;
    std::unordered_map<std::string, ::CompoundTag>        mModifiedVanillaItems;
    ll::memory::HookRegistrar<RegisterItemsHook, ClientItemActorFixHook, VanillaItemDefinitionSendHook> mHooks;
};

LL_STATIC_HOOK(
    RegisterItemsHook,
    HookPriority::Normal,
    &::VanillaItems::registerItems,
    void,
    ::cereal::ReflectionCtx& ctx,
    ::ItemRegistryRef        itemRegistry, /*NOLINT*/
    ::BaseGameVersion const& baseGameVersion,
    ::Experiments const&     experiments
) {
    origin(ctx, itemRegistry, baseGameVersion, experiments);
    auto& manager     = *CustomItemRegistry::getInstance().pImpl;
    manager.mRegistry = itemRegistry._lockRegistry().get();
    auto& registry    = *manager.mRegistry;
    for (auto& func : manager.mPendingItems) {
        try {
            auto  item   = func().release();
            auto  name   = item->mFullName;
            short itemId = 0;
            if (auto existItem = registry.lookupByNameNoAlias(name->getString())) {
                item->mId = existItem->mId;
                itemId    = existItem->mId;
                registry.unregisterItem(name);
            } else {
                registry.mMaxItemID++;
                itemId    = registry.mMaxItemID;
                item->mId = itemId;
            }
            registry.mItemRegistry->emplace_back(item);
            auto& sharedItem                 = registry.mItemRegistry->at(registry.mItemRegistry->size() - 1);
            (*registry.mIdToItemMap)[itemId] = sharedItem;
            (*registry.mNameToItemMap)[name] = sharedItem;
            for (auto& tag : *sharedItem->mTags) {
                (*registry.mTagToItemsMap)[tag].insert(sharedItem.get());
            }
        } catch (...) {}
    }
    manager.mPendingItems.clear();
}

CustomItemRegistry::CustomItemRegistry() : pImpl(std::make_unique<Impl>()) {}

CustomItemRegistry& CustomItemRegistry::_registerItem(std::function<std::unique_ptr<Item>()>&& func) {
    if (pImpl->mRegistry) {
        try {
            auto& registry   = *pImpl->mRegistry;
            auto  item       = func().release();
            auto  name       = item->mFullName;
            short itemId     = 0;
            bool  registered = false;
            if (auto existItem = registry.lookupByNameNoAlias(name->getString())) {
                item->mId = existItem->mId;
                itemId    = existItem->mId;
                registry.unregisterItem(name);
                registered = true;
            } else {
                registry.mMaxItemID++;
                itemId    = registry.mMaxItemID;
                item->mId = itemId;
            }
            registry.mItemRegistry->emplace_back(item);
            auto& sharedItem                 = registry.mItemRegistry->at(registry.mItemRegistry->size() - 1);
            (*registry.mIdToItemMap)[itemId] = sharedItem;
            (*registry.mNameToItemMap)[name] = sharedItem;
            for (auto& tag : *sharedItem->mTags) {
                (*registry.mTagToItemsMap)[tag].insert(sharedItem.get());
            }
            if (!registered) {
                if (sharedItem->mIsHiddenInCommands == ::ItemCommandVisibility::Visible) {
                    auto registerCommandEnum = static_cast<int (*)(
                        CommandRegistry*,
                        std::string const&,
                        std::vector<std::pair<std::string, ::CommandItem>> const&
                    )>(
                        ll::memory::SymbolView(
                            "??$addEnumValues@VCommandItem@@U?$DefaultIdConverter@VCommandItem@@@CommandRegistry@@@"
                            "CommandRegistry@@QEAAHAEBV?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@"
                            "AEBV?$vector@U?$pair@V?$basic_string@DU?$char_traits@D@std@@V?$allocator@D@2@@std@@"
                            "VCommandItem@@@std@@V?$allocator@U?$pair@V?$basic_string@DU?$char_traits@D@std@@V?$"
                            "allocator@D@2@@std@@VCommandItem@@@std@@@2@@2@@Z"
                        )
                            .resolve()
                    );
                    registerCommandEnum(
                        ll::service::getCommandRegistry().as_ptr(),
                        "Item",
                        {
                            {sharedItem->mFullName->getString(), ::CommandItem{{{1, false, sharedItem->mId}}}}
                    }
                    );
                    /*
                    函数原型是
                    其中：
                    class CommandRegistry {
                    public:
                        template <typename T>
                        struct DefaultIdConverter{};

                        template <typename T, typename C>
                        int addEnumValues(
                            std::string const& enumName,
                            const std::vector<std::pair<std::string, T>>& values
                        );
                    };
                    ////////
                    ll::service::getCommandRegistry()
                        ->addEnumValues<CommandItem, CommandRegistry::DefaultIdConverter<CommandItem>>(
                            "Item",
                            {{sharedItem->getSerializedName(), ::CommandItem(1, 0, sharedItem->mId)}
                        });
                    */
                }
                if ((int)sharedItem->mCreativeCategory >= 1 && (int)sharedItem->mCreativeCategory <= 4) {
                    CustomCreativeItemRegistry::getInstance().registerCreativeItem(
                        ::ItemInstance(sharedItem->getSerializedName()),
                        sharedItem->mCreativeCategory,
                        *sharedItem->mCreativeGroup
                    );
                }
            }
        } catch (...) {}
    } else {
        pImpl->mPendingItems.push_back(std::move(func));
    }
    return *this;
}

CustomItemRegistry& CustomItemRegistry::_modifyItem(std::function<void(CustomItemRegistry& registry)>&& func) {
    pImpl->mPendingModifyItems.push_back(std::move(func));
    return *this;
}

CustomItemRegistry& CustomItemRegistry::getInstance() {
    static CustomItemRegistry instance;
    if (!instance.pImpl->mRegistry) {
        if (auto level = ll::service::getLevel()) {
            instance.pImpl->mRegistry = level->getItemRegistry()._lockRegistry().get();
        }
    }
    return instance;
}

void CustomItemRegistry::forEachItemInRegistry(std::function<bool(Item& item)>&& func) {
    if (pImpl->mRegistry) {
        bool status = true;
        for (auto& item : *pImpl->mRegistry->mItemRegistry) {
            try {
                status = func(*item);
            } catch (...) {}
            if (!status) return;
        }
    }
}

LL_TYPE_INSTANCE_HOOK(
    ClientItemActorFixHook,
    HookPriority::Normal,
    ::Spawner,
    &::Spawner::$spawnItem,
    ::ItemActor*,
    ::BlockSource&     region,
    ::ItemStack const& inst,
    ::Actor*           spawner,
    ::Vec3 const&      pos,
    int                throwTime
) {
    auto itemActor = origin(region, inst, spawner, pos, throwTime);
    if (itemActor && !itemActor->item().isNull()) {
        auto& dataFlagComponent = itemActor->getEntityContext().getOrAddComponent<ActorDataFlagComponent>();
        dataFlagComponent.mValue.set(
            std::to_underlying(ActorFlags::FireImmune),
            itemActor->item().getItem()->mFireResistant
        );
        itemActor->getEntityContext().mEnTTRegistry.emplace<ActorDataDirtyFlagsComponent>(
            itemActor->getEntityContext().mEntity
        );
    }
    return itemActor;
}

::WeakPtr<::Item> CustomItemRegistry::getItem(std::string_view name) {
    if (pImpl->mRegistry) {
        return pImpl->mRegistry->lookupByNameNoAlias(name);
    }
    return {};
}

bool CustomItemRegistry::setFireResistant(std::string_view itemName, bool value) {
    if (auto item = getItem(itemName)) {
        item->mFireResistant = value;
        return true;
    }
    return false;
}

bool CustomItemRegistry::addTag(std::string_view itemName, std::string_view tag) {
    if (auto item = getItem(itemName)) {
        item->addTag(::ItemTag(std::string{tag}));
        auto& nbt        = getAndModifyVanillaNetworkTagInfo(itemName);
        nbt["item_tags"] = ::ListTag();
        for (auto& itemTag : *item->mTags) {
            nbt["item_tags"].push_back(itemTag.getString());
        }
        return true;
    }
    return false;
}

bool CustomItemRegistry::setIcon(std::string_view itemName, std::string_view icon) {
    if (auto item = getItem(itemName)) {
        auto& nbt                                                       = getAndModifyVanillaNetworkTagInfo(itemName);
        nbt["item_properties"]["minecraft:icon"]["textures"]["default"] = icon;
        return true;
    }
    return false;
}

bool CustomItemRegistry::setDisplayName(std::string_view itemName, std::string_view displayName) {
    if (auto item = getItem(itemName)) {
        auto& nbt                              = getAndModifyVanillaNetworkTagInfo(itemName);
        nbt["minecraft:display_name"]["value"] = displayName;
        return true;
    }
    return false;
}

bool CustomItemRegistry::setRepairItem(std::string_view itemName, std::string_view fixItem) {
    if (auto item = getItem(itemName)) {
        auto& nbt                                   = getAndModifyVanillaNetworkTagInfo(itemName);
        nbt["minecraft:repairable"]["repair_items"] = ::ListTag({
            {{"items", ::ListTag({{{"name", fixItem}}})}, {"repair_amount", "q.max_durability * 0.25"}}
        });
        return true;
    }
    return false;
}

::CompoundTag& CustomItemRegistry::getAndModifyVanillaNetworkTagInfo(std::string_view itemName) {
    ::CompoundTag nbt;
    if (pImpl->mModifiedVanillaItems.contains(std::string(itemName))) {
        return pImpl->mModifiedVanillaItems.at(std::string(itemName));
    }
    if (auto item = getItem(itemName)) {
        if (item->mItemParseVersion != ::ItemVersion::DataDriven) {
            item->mItemParseVersion = ::ItemVersion::DataDriven;
            if (auto netTag = item->buildNetworkTag()) {
                nbt = *netTag;
            }
            nbt["item_properties"]["allow_off_hand"]          = item->mAllowOffhand;
            nbt["item_properties"]["can_destroy_in_creative"] = item->canDestroyInCreative();
            nbt["item_properties"]["creative_category"]       = (int)item->mCreativeCategory;
            nbt["item_properties"]["creative_group"]          = *item->mCreativeGroup;
            nbt["item_properties"]["damage"]                  = item->getAttackDamage();
            nbt["item_properties"]["enchantable_slot"]  = buildEnchantSlot(::Enchant::Slot(item->getEnchantSlot()));
            nbt["item_properties"]["enchantable_value"] = item->getEnchantValue();
            nbt["item_properties"]["foil"]              = item->mIsGlint;
            nbt["item_properties"]["hand_equipped"]     = item->isHandEquipped();
            nbt["item_properties"]["liquid_clipped"]    = item->isLiquidClipItem();
            nbt["item_properties"]["max_stack_size"]    = (int)item->mMaxStackSize;
            nbt["item_properties"]["minecraft:icon"]["textures"]["default"] =
                ll::string_utils::replaceAll(std::string(itemName), "minecraft:", "");
            nbt["item_properties"]["mining_speed"]    = 1.0f;
            nbt["item_properties"]["should_despawn"]  = item->mShouldDespawn;
            nbt["item_properties"]["stacked_by_data"] = item->isStackedByData();
            nbt["item_properties"]["use_animation"]   = (int)item->mUseAnim;
            nbt["item_properties"]["use_duration"]    = item->mMaxUseDuration;
            nbt["item_tags"]                          = ::ListTag();
            for (auto& itemTag : *item->mTags) {
                nbt["item_tags"].push_back(itemTag.getString());
            }
            nbt["minecraft:display_name"]["value"] =
                "item." + ll::string_utils::replaceAll(std::string(itemName), "minecraft:", "") + ".name";
            if (item->mMaxDamage > 0) {
                nbt["minecraft:durability"] = {
                    {"damage_chance",  {{"max", 100}, {"min", 100}}},
                    {"max_durability", (int)item->mMaxDamage       }
                };
            }
        }
    }
    pImpl->mModifiedVanillaItems[std::string(itemName)] = std::move(nbt);
    return pImpl->mModifiedVanillaItems.at(std::string(itemName));
}

LL_TYPE_INSTANCE_HOOK(
    VanillaItemDefinitionSendHook,
    HookPriority::Normal,
    ItemRegistryPacket,
    &ItemRegistryPacket::$write,
    void,
    BinaryStream& stream
) {
    auto& items            = mItems;
    auto& itemsModifiedMap = CustomItemRegistry::getInstance().pImpl->mModifiedVanillaItems;
    for (auto& item : *items) {
        if (itemsModifiedMap.contains(*item.mName)) {
            item.mComponentData->at("components") = itemsModifiedMap.at(*item.mName);
        }
    }
    return origin(stream);
}

} // namespace modapi::inline item