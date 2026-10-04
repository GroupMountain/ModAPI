#pragma once
#include "modapi/Macros.h"
#include "modapi/item/types/ItemIcon.h"
#include <mc/deps/core/string/HashedString.h>
#include <mc/deps/nbt/CompoundTag.h>
#include <mc/world/item/BlockItem.h>
#include <string>
#include <vector>

class BlockType;

namespace modapi::inline item {

// An item that places a custom block: it *is* an engine `::BlockItem` - that class carries the placement
// behaviour - with the conveniences a plain custom item gets from `ICustomItem`, applied to the engine item
// by `_init()` (which `ItemRegistry` calls right after construction, exactly like it does for `ICustomItem`).
//
//   class MyBlockItem : public modapi::item::ICustomBlockItem {
//   public:
//       MyBlockItem(std::string const& identifier) : ICustomBlockItem(identifier, ::HashedString{"mymod:my_block"}) {}
//
//       uint8_t getItemMaxStackSize() const override { return 16; }
//   };
//   static modapi::DeferredRegister<modapi::ItemRegistry, MyBlockItem> gMyBlockItem{std::string{"mymod:my_block"}};
//
// The block is linked purely by name, which is how vanilla does it too: `::BlockItem` knows the block it
// places and derives its icon from it, so no icon has to be described here.
class ICustomBlockItem : public ::BlockItem {
public:
    // The item id is allocated by `ItemRegistry`, so only the name and the block are given here.
    MOD_API ICustomBlockItem(std::string const& identifier, ::HashedString const& blockName);

    // Same, taking the block itself (its full name is what the engine keys on).
    MOD_API ICustomBlockItem(std::string const& identifier, ::BlockType const& block);

    MOD_API ~ICustomBlockItem() override;

    // No icon hook, on purpose: a block item's picture is the block itself. The definition it is registered with
    // carries `minecraft:block_placer` with `canUseBlockAsIcon` and no `minecraft:icon` at all (see
    // `NetworkTagBuilder`), so a client draws the block - `::BlockItem::getIconInfo` resolves it from the block.

    // The name the client shows; empty means "use the item's identifier and let a language file name it".
    MOD_API virtual std::string getDisplayName() const;

    MOD_API virtual uint8_t                  getItemMaxStackSize() const;
    MOD_API virtual std::vector<std::string> getItemTags() const;
    MOD_API virtual std::string              getHoverTextColorFormat() const;
    MOD_API virtual bool                     shouldDespawn() const;
    MOD_API virtual bool                     isFoil() const;

    // Which creative tab lists this item. The engine builds the creative list from the item registry itself,
    // so an item without a category is never listed - this is what puts the item in a client's inventory, and
    // it is why the creative entries are not queued by `ItemRegistry` separately.
    MOD_API virtual ::SharedTypes::CreativeItemCategory getCreativeCategory() const;
    MOD_API virtual std::string                         getCreativeGroup() const;

    // The block this item belongs to, kept so `_init` can link the item to it.
    std::string mBlockName;

    // The client side of the item: what `ItemRegistryPacket` sends for it.
    MOD_API std::unique_ptr<::CompoundTag> buildNetworkTag() const override;

    // Applies the overrides above onto this item.
    MOD_API void _init();
};

} // namespace modapi::inline item
