#include <gmlib/mod/item/CustomCreativeItemRegistry.h>
#include <ll/api/service/Bedrock.h>
#include <mc/world/item/registry/CreativeGroupInfo.h>
#include <mc/world/item/registry/CreativeItemEntry.h>
#include <mc/world/item/registry/CreativeItemGroupCategory.h>
#include <mc/world/item/registry/CreativeItemRegistry.h>
#include <mc/world/item/registry/ItemRegistry.h>
#include <mc/world/item/registry/ItemRegistryRef.h>
#include <mc/world/level/Level.h>

namespace std {

template <>
class hash<CreativeItemNetId> {
public:
    size_t operator()(CreativeItemNetId const& netId) const { return std::hash<uint32_t>{}(netId.mRawId); }
};

} // namespace std

bool operator==(CreativeItemNetId const& lhs, CreativeItemNetId const& rhs) { return lhs.mRawId == rhs.mRawId; }

::CreativeGroupInfo& ::CreativeGroupInfo::operator=(::CreativeGroupInfo const&) = default;

namespace gmlib::mod {

struct CustomCreativeItemRegistry::Impl {
    CreativeItemRegistry* mRegistry;
};

CustomCreativeItemRegistry::CustomCreativeItemRegistry() : pImpl(std::make_unique<Impl>()) {}

CustomCreativeItemRegistry& CustomCreativeItemRegistry::getInstance() {
    static CustomCreativeItemRegistry instance;
    if (!instance.pImpl->mRegistry) {
        if (auto level = ll::service::getLevel()) {
            instance.pImpl->mRegistry = level->getItemRegistry()._lockRegistry()->mCreativeItemRegistry.get();
        }
    }
    return instance;
}

uint32_t CustomCreativeItemRegistry::registerCreativeGroup(
    std::string_view       groupName,
    ::ItemInstance&&       icon,
    ::CreativeItemCategory category
) {
    if (!groupName.empty()) {
        auto& existedGroups = *pImpl->mRegistry->mCreativeGroupCategories->at(category).mNamedGroupIndex;
        if (existedGroups.contains(::HashedString(groupName))) {
            return existedGroups.at(::HashedString(groupName));
        }
    }
    auto nextIndex = (uint32_t)pImpl->mRegistry->mCreativeGroups->size();
    pImpl->mRegistry->mCreativeGroups
        ->emplace_back(pImpl->mRegistry, category, ::HashedString(groupName), nextIndex, icon);
    pImpl->mRegistry->mCreativeGroupCategories->at(category).mGroupIndexes->push_back(nextIndex);
    if (!groupName.empty()) {
        pImpl->mRegistry->mCreativeGroupCategories->at(category).mNamedGroupIndex->emplace(
            ::HashedString(groupName),
            nextIndex
        );
    }
    return nextIndex;
}

bool CustomCreativeItemRegistry::registerCreativeItem(
    ::ItemInstance&&       item,
    ::CreativeItemCategory category,
    std::string_view       itemGroup
) {
    if ((int)category >= 1 && (int)category <= 4) {
        uint32_t groupIndex    = registerCreativeGroup(itemGroup, ::ItemInstance(), category);
        auto     nextNetworkId = (uint32_t)pImpl->mRegistry->mCreativeNetIdIndex->size() + 1;
        auto     nextIndex     = (uint32_t)pImpl->mRegistry->mCreativeItems->size();
        auto     entry         = ::CreativeItemEntry(pImpl->mRegistry, nextNetworkId, item, nextIndex);
        entry.mGroupIndex      = groupIndex;
        pImpl->mRegistry->mCreativeItems->push_back(std::move(entry));
        pImpl->mRegistry->mCreativeNetIdIndex->emplace(nextNetworkId, nextIndex);
        return true;
    }
    return false;
}

std::vector<CustomCreativeItemRegistry::CreativeItem>
CustomCreativeItemRegistry::getCreativeItem(std::string_view itemType) {
    std::vector<CustomCreativeItemRegistry::CreativeItem> result;

    auto& groups = *pImpl->mRegistry->mCreativeGroups;
    for (auto& entry : *pImpl->mRegistry->mCreativeItems) {
        if (entry.mItemInstance->getTypeName() == itemType) {
            result.emplace_back(
                entry.mItemInstance,
                groups.at(entry.mGroupIndex).mCategory,
                groups.at(entry.mGroupIndex).mName->getString()
            );
        }
    }
    return result;
}

bool CustomCreativeItemRegistry::unregisterCreativeItem(CustomCreativeItemRegistry::CreativeItem const& item) {
    auto& groups = *pImpl->mRegistry->mCreativeGroups;
    for (auto it = pImpl->mRegistry->mCreativeItems->begin(); it != pImpl->mRegistry->mCreativeItems->end(); ++it) {
        if (&(*it->mItemInstance) == &item.mItem) {
            auto groupIndex = it->mGroupIndex;
            if (groups.at(groupIndex).mCategory == item.mCategory
                && groups.at(groupIndex).mName->getString() == item.mGroup) {
                size_t entryIndex = std::distance(pImpl->mRegistry->mCreativeItems->begin(), it);
                for (auto& [netId, index] : *pImpl->mRegistry->mCreativeNetIdIndex) {
                    if (index > entryIndex) index--;
                }
                pImpl->mRegistry->mCreativeItems->erase(it);
                return true;
            }
        }
    }
    return false;
}

} // namespace gmlib::mod