#include "modapi/item/base/ICustomBlockItem.h"
#include "modapi/block/BlockRegistry.h"
#include "modapi/core/Gloabl.h"
#include "modapi/item/ItemRegistry.h"
#include "modapi/item/shared_types/ItemInitializer.h"
#include <mc/world/level/block/BlockType.h>
#include <mc/world/level/block/registry/BlockTypeRegistry.h>

namespace modapi::inline item {
namespace {

::HashedString const& blockNameOf(::BlockType const& block) { return *block.mNameInfo->mFullName; }

// Vanilla's own rule, read out of `ItemRegistryRef::registerBlockItem<BlockItem>`: the item for a block is built
// as `BlockItem(name, foldedBlockId, blockName)`, with the block's id folded into a short
// (`id <= 0xFF ? id : 255 - id`). The engine builds its own item for an addon block exactly that way (measured:
// block id 10002 -> item id -9747); handing `::BlockItem` anything else leaves its internal state disagreeing
// with `mId`, which is what produced a creative slot nothing could be done with.
short itemIdOf(::HashedString const& blockName) {
    auto const* block = ::BlockTypeRegistry::get().lookupByName(blockName, false);
    if (block == nullptr) return 0;
    auto const raw = static_cast<ushort>((*block->mID).mValue);
    return raw <= 0xFF ? static_cast<short>(raw) : static_cast<short>(255 - raw);
}

} // namespace

ICustomBlockItem::ICustomBlockItem(std::string const& identifier, ::HashedString const& blockName)
: ::BlockItem(identifier, itemIdOf(blockName), blockName) {
    // Required: without it a client does not accept this item at all (measured - the item simply disappeared from
    // the creative inventory). With it the client treats the item as a `ComponentItem`, so its icon has to come
    // from an icon component, which draws a flat texture: a three dimensional icon is only produced for an item a
    // client links to a block, and that is what the engine's own item for a block gets.
    mItemParseVersion = ItemVersion::DataDriven;
    mBlockName        = blockName.getString();
}

ICustomBlockItem::ICustomBlockItem(std::string const& identifier, ::BlockType const& block)
: ICustomBlockItem(identifier, blockNameOf(block)) {}

ICustomBlockItem::~ICustomBlockItem() = default;

std::string ICustomBlockItem::getDisplayName() const { return {}; }

std::unique_ptr<::CompoundTag> ICustomBlockItem::buildNetworkTag() const { return buildClientComponents(*this); }

uint8_t ICustomBlockItem::getItemMaxStackSize() const { return 64; }

std::vector<std::string> ICustomBlockItem::getItemTags() const { return {}; }

std::string ICustomBlockItem::getHoverTextColorFormat() const { return {}; }

bool ICustomBlockItem::shouldDespawn() const { return true; }

bool ICustomBlockItem::isFoil() const { return false; }

::SharedTypes::CreativeItemCategory ICustomBlockItem::getCreativeCategory() const {
    return ::SharedTypes::CreativeItemCategory::Items;
}

std::string ICustomBlockItem::getCreativeGroup() const { return {}; }

void ICustomBlockItem::_init() {
    initCustomItem(*this);

    // `::BlockItem` does not link itself to the block (`getBlockTypeForRendering` is not even overridden by
    // it): the link is `::Item::mBlockType`, and without setting it here the item reports `minecraft:air` and
    // places nothing. The block exists by this point - block types are registered before items are built.
    if (auto const* block = ::BlockTypeRegistry::get().lookupByName(::HashedString{mBlockName}, false)) {
        mBlockType = block;
    } else {
        core::getLogger().error(
            "ICustomBlockItem[{}]: no block named '{}'; the item will not place anything.",
            mFullName->getString(),
            mBlockName
        );
    }
}

} // namespace modapi::inline item
