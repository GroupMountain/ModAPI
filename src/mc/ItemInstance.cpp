#include <mc/world/item/ItemInstance.h>
#include <mc/world/item/ItemStackBase.h>

// Definitions for symbols not exported by the server SDK.
//
// The server header declares `ItemInstance()` without an exported definition; `ItemStackBase()` is
// still MCAPI, so the base call links fine.

ItemInstance::ItemInstance() : ItemStackBase() {}

ItemInstance& ItemInstance::operator=(ItemInstance const& rhs) {
    ItemStackBase::operator=(rhs);
    return *this;
}
