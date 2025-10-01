#pragma once
#include <gmlib/Macros.h>
#include <mc/world/item/CreativeItemCategory.h>
#include <mc/world/item/ItemInstance.h>

namespace gmlib::mod {

class CustomCreativeItemRegistry {
    struct Impl;
    std::unique_ptr<Impl> pImpl;

public:
    struct CreativeItem {
        ::ItemInstance&        mItem;
        ::CreativeItemCategory mCategory;
        std::string            mGroup;
    };

public:
    CustomCreativeItemRegistry();
    CustomCreativeItemRegistry& operator=(CustomCreativeItemRegistry const&) = delete;
    CustomCreativeItemRegistry(CustomCreativeItemRegistry const&)            = delete;

public:
    MODAPI_NDAPI static CustomCreativeItemRegistry& getInstance();

public:
    MOD_API uint32_t registerCreativeGroup(
        std::string_view       groupName,
        ::ItemInstance&&       icon     = ::ItemInstance(),
        ::CreativeItemCategory category = ::CreativeItemCategory::Items
    );

    MOD_API bool registerCreativeItem(
        ::ItemInstance&&       item,
        ::CreativeItemCategory category  = ::CreativeItemCategory::Items,
        std::string_view       itemGroup = {}
    );

    MODAPI_NDAPI std::vector<CreativeItem> getCreativeItem(std::string_view itemType);

    MOD_API bool unregisterCreativeItem(CreativeItem const& item);
};

} // namespace gmlib::mod