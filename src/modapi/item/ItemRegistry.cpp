#pragma include_alias("mc/deps/shared_types/util/Reference.h", "modapi/item/ReferencePatched.h")
#include "modapi/item/ItemRegistry.h"
#include "modapi/block/BlockRegistry.h"
#include "modapi/core/RegistryEvent.h"
#include "modapi/item/CreativeItemRegistry.h"
#include "modapi/item/shared_types/NetworkTagBuilder.h"
#include <atomic>
#include <cstring>
#include <ll/api/command/CommandRegistrar.h>
#include <ll/api/event/EventBus.h>
#include <ll/api/memory/Hook.h>
#include <ll/api/service/Bedrock.h>
#include <ll/api/utils/ErrorUtils.h>
#include <mc/common/SharedPtr.h>
#include <mc/deps/core/utility/optional_ref.h>
#include <mc/deps/vanilla_components/ActorDataDirtyFlagsComponent.h>
#include <mc/deps/vanilla_components/ActorDataFlagComponent.h>
#include <mc/network/packet/ItemData.h>
#include <mc/network/packet/ItemRegistryPacket.h>
#include <mc/network/packet/cerealize/core/SerializationMode.h>
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

namespace {

int registerCommandItemEnum(
    CommandRegistry*                                          registry,
    std::string const&                                        name,
    std::vector<std::pair<std::string, ::CommandItem>> const& values
) {
    std::vector<std::pair<std::string, uint64>> converted;
    converted.reserve(values.size());
    for (auto& [str, item] : values) {
        converted.emplace_back(str, item.mVersionId);
    }
    const auto id     = registry->mEnumLookup["Item"];
    auto       parser = registry->mEnums[id].parse;
    auto       symbol = registry->_addEnumValuesInternal(
        name,
        converted,
        Bedrock::typeid_storage_impl<CommandRegistry, ::CommandItem>().get<CommandRegistry>(),
        parser
    );
    return symbol.mValue;
}

} // namespace

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

struct ItemRegistry::Impl {
    ll::memory::HookRegistrar<RegisterItemsHook, ClientItemActorFixHook, VanillaItemDefinitionSendHook> mHooks;
    // Bound by the hook below, once the vanilla item pass has run.
    ::ItemRegistry*                                mRegistry = nullptr;
    bool                                           mReady    = false;
    std::unordered_map<std::string, ::CompoundTag> mModifiedVanillaItems;
    // Every item this API registered. The engine builds no definition for those, so their entry in the packet
    // that carries item definitions to a client has to be filled in from `buildNetworkTag()`.
    std::unordered_set<std::string> mCustomItemNames;
};

// `ItemRegistryRef::initServer` calls `VanillaItems::registerItems` and then moves the pre-registry
// into the main registry (`ItemRegistry::_movePreRegistryToMainRegistry`) before
// `ItemRegistry::finishedRegistration`. Custom items therefore have to be injected here.
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
    if (auto registry = itemRegistry.mWeakRegistry.lock()) {
        ItemRegistry::getInstance()._bindRegistry(*registry);
    }
}

ItemRegistry::ItemRegistry() : pImpl(std::make_unique<Impl>()) {}

ItemRegistry& ItemRegistry::getInstance() {
    // Deliberately leaked: the hook registrar this instance owns must not unregister its hooks
    // while the process/DLL is being torn down.
    static ItemRegistry& instance = *new ItemRegistry();
    return instance;
}

bool ItemRegistry::isReady() const noexcept { return pImpl->mReady; }

void ItemRegistry::ensureEventRegistered() {
    // The vanilla hooks live in the registry's implementation, so the singleton has to exist before
    // the pass that fires them; every consumer goes through this entry point.
    (void)getInstance();
    static std::atomic_bool registered = false;
    core::ensureRegistryEventRegistered<EventType>(registered);
}

void ItemRegistry::_bindRegistry(::ItemRegistry& registry) {
    pImpl->mRegistry = &registry;
    pImpl->mReady    = true;
    ll::event::EventBus::getInstance().publish(ItemReadyEvent{*this});
}

ItemRegistry::ProductRef ItemRegistry::_registerItem(std::function<std::unique_ptr<::Item>()>&& func) {
    if (!pImpl->mReady || pImpl->mRegistry == nullptr) {
        core::getLogger().error(
            "ItemRegistry: items can only be registered during the vanilla item pass; "
            "use DeferredRegister<ItemRegistry, ...> to register items before the server starts."
        );
        return {};
    }
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
            // Drop the replaced item from the registry vector as well. Leaving it there kept a dead entry
            // alive for every pass that walks that vector (the client definition build, and this API's own
            // `forEachItemInRegistry`), so two items answered to the same name - the leftover one with none of
            // the values its mod had applied, because the second registration was the one that took effect.
            auto& items = *registry.mItemRegistry;
            for (auto it = items.begin(); it != items.end(); ++it) {
                if ((*it).get() != nullptr && (*it)->mFullName->getString() == name->getString()) {
                    items.erase(it);
                    break;
                }
            }
        } else if (item->mId != 0) {
            // The item reserved its id before it was built: a block item needs its final
            // id while `::BlockItem` runs, otherwise its state does not match `mId` and the engine lists a
            // creative entry that cannot be operated.
            itemId = item->mId;
        } else {
            registry.mMaxItemID++;
            itemId    = registry.mMaxItemID;
            item->mId = itemId;
        }
        // The id has to be handed out here: neither `_preRegisterItem` nor `registerItem` allocates one (both
        // leave the item at id 0), so the engine's own routes cannot be used as they are.
        registry.mItemRegistry->emplace_back(item);
        auto& sharedItem                 = registry.mItemRegistry->at(registry.mItemRegistry->size() - 1);
        (*registry.mIdToItemMap)[itemId] = sharedItem;
        (*registry.mNameToItemMap)[name] = sharedItem;
        for (auto& tag : *sharedItem->mTags) {
            (*registry.mTagToItemsMap)[tag].insert(sharedItem.get());
        }
        // Remembered so the item definition packet can be filled in for it (the engine has none).
        pImpl->mCustomItemNames.insert(name->getString());

        // A block item is listed by the engine itself, because it carries a creative category: `ICustomBlockItem`
        // hands one over and `ItemInitializer` writes it onto the item. Nothing has to be queued for it.
        if (!registered) {
            if (sharedItem->mIsHiddenInCommands == ::ItemCommandVisibility::Visible) {
                registerCommandItemEnum(
                    ll::service::getCommandRegistry().as_ptr(),
                    "Item",
                    {
                        // `CommandItem` is `{mVersion, mOverrideAux, mId}` (a union packed into
                        // `mVersionId`). `mVersion` has to be non-zero: `CommandItem::createInstance` only takes
                        // `mId` as the item's own id then - with 0 it converts the value as a *legacy* id through
                        // a different table, which resolves the id to the wrong item.
                        {sharedItem->mFullName->getString(), {{{1, true, (int)sharedItem->mId}}}}
                }
                );
            }
        }
        return sharedItem.get();
    } catch (...) {
        return {};
    }
}


ItemRegistry& ItemRegistry::_modifyItem(std::function<void(ItemRegistry& registry)>&& func) {
    if (!pImpl->mReady) {
        core::getLogger().error(
            "ItemRegistry: vanilla items can only be modified once the item pass has run; "
            "use DeferredRegister<ItemRegistry, ...> instead."
        );
        return *this;
    }
    try {
        func(*this);
    } catch (...) {}
    return *this;
}

void ItemRegistry::forEachItemInRegistry(std::function<bool(Item& item)>&& func) {
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
            itemActor->item().mItem->mFireResistant
        );
        itemActor->getEntityContext().mEnTTRegistry.emplace<ActorDataDirtyFlagsComponent>(
            itemActor->getEntityContext().mEntity
        );
    }
    return itemActor;
}

::WeakPtr<::Item> ItemRegistry::getItem(std::string_view name) {
    if (pImpl->mRegistry) {
        return pImpl->mRegistry->lookupByNameNoAlias(name);
    }
    return {};
}

bool ItemRegistry::setFireResistant(std::string_view itemName, bool value) {
    if (auto item = getItem(itemName)) {
        item->mFireResistant = value;
        return true;
    }
    return false;
}

bool ItemRegistry::addTag(std::string_view itemName, std::string_view tag) {
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

bool ItemRegistry::setIcon(std::string_view itemName, std::string_view icon) {
    if (auto item = getItem(itemName)) {
        auto& nbt                                                       = getAndModifyVanillaNetworkTagInfo(itemName);
        nbt["item_properties"]["minecraft:icon"]["textures"]["default"] = icon;
        return true;
    }
    return false;
}

bool ItemRegistry::setDisplayName(std::string_view itemName, std::string_view displayName) {
    if (auto item = getItem(itemName)) {
        auto& nbt                              = getAndModifyVanillaNetworkTagInfo(itemName);
        nbt["minecraft:display_name"]["value"] = displayName;
        return true;
    }
    return false;
}

bool ItemRegistry::setRepairItem(std::string_view itemName, std::string_view fixItem) {
    if (auto item = getItem(itemName)) {
        auto& nbt                                   = getAndModifyVanillaNetworkTagInfo(itemName);
        nbt["minecraft:repairable"]["repair_items"] = ::ListTag({
            {{"items", ::ListTag({{{"name", fixItem}}})}, {"repair_amount", "q.max_durability * 0.25"}}
        });
        return true;
    }
    return false;
}

::CompoundTag& ItemRegistry::getAndModifyVanillaNetworkTagInfo(std::string_view itemName) {
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
            nbt["item_properties"]["allow_off_hand"]          = isOffhandAllowed(item->mAllowOffhand);
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

// Where an item's definition for a client is written. Two hooks call the same code: the payload constructor
// produces `mItems` (every sending path goes through it), and `writeWithSerializationMode` is the serialization
// different virtual and hooking it (which this did) never ran, so no custom item definition ever reached a
// client and the client reported "requires either an icon atlas or icon texture".
void prepareItemDefinitions(::std::vector<::ItemData>& items) {
    auto& impl = *ItemRegistry::getInstance().pImpl;

    int custom = 0;
    for (auto& item : items) {
        if (impl.mCustomItemNames.contains(*item.mName)) ++custom;
    }

    // A client only registers a block item when it is in the payload the server sends, so which block items are in
    // there is worth naming: the engine builds an item for every block definition it has, and whether one of them
    // actually reaches a client is not something a mod can see anywhere else.
    for (auto& item : items) {
        if (BlockRegistry::getInstance().getBlock(item.mName->getString())) {}
    }

    // Merge a description into what the engine publishes for that item, instead of replacing the whole node:
    // the sources below are built from `Item::buildNetworkTag()`, which for a data driven item carries almost
    // replacing `components` dropped everything the item had.
    auto merge = [](::CompoundTag& published, ::CompoundTag const& source) {
        for (auto const& [key, value] : source) {
            if (key == "item_properties" && published.mTags.contains(key)) {
                for (auto const& [propertyKey, propertyValue] : value.get<::CompoundTag>().mTags) {
                    published[key][propertyKey] = propertyValue;
                }
            } else {
                published[key] = value;
            }
        }
    };

    for (auto& item : items) {
        // The node may not hold a compound yet - a custom item's entry is nearly empty - and reading it as one
        // throws "bad variant access", which aborted this whole merge on every custom item, so nothing ever
        // reached a client.
        ::CompoundTag* components = nullptr;
        try {
            components = &(*item.mComponentData)["components"].get<::CompoundTag>();
        } catch (...) {
            components = nullptr;
        }
        if (components == nullptr) {
            (*item.mComponentData)["components"] = ::CompoundTag{};
            components                           = &(*item.mComponentData)["components"].get<::CompoundTag>();
        }
        auto& published = *components;

        // An item this API registered: the engine parses no document for it, so its entry carries almost
        // nothing. This is the only place that definition can still be added.
        if (impl.mCustomItemNames.contains(*item.mName)) {
            if (auto customItem = ItemRegistry::getInstance().getItem(*item.mName)) {
                if (auto tag = customItem->buildNetworkTag()) {
                    merge(published, *tag);
                }
            }
        }

        // A vanilla item a mod modified through the setters.
        if (auto modified = impl.mModifiedVanillaItems.find(*item.mName);
            modified != impl.mModifiedVanillaItems.end()) {
            merge(published, modified->second);
        }
    }
}

LL_TYPE_INSTANCE_HOOK(
    VanillaItemDefinitionSendHook,
    HookPriority::Normal,
    ItemRegistryPacket,
    &ItemRegistryPacket::$writeWithSerializationMode,
    void,
    BinaryStream&                        stream,
    ::cereal::ReflectionCtx const&       reflectionCtx,
    ::std::optional<::SerializationMode> overrideMode
) {
    prepareItemDefinitions(*mItems);
    return origin(stream, reflectionCtx, overrideMode);
}


ItemRegistry& ItemReadyEvent::registry() const { return mRegistry; }

} // namespace modapi::inline item
