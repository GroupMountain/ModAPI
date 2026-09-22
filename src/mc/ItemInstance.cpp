#include <mc/world/item/ItemInstance.h>
#include <mc/world/item/ItemStackBase.h>

// Definitions for symbols not exported in LeviLamina 26.51.x server.
//
// 26.40 still exported `ItemInstance::ItemInstance()` (the prelink resolved it out of the archives),
// 26.51 only declares it for the client - the server branch of the header has a bare, undefined
// `ItemInstance();`. `ItemStackBase()` itself is still MCAPI, so the base call links fine.

ItemInstance::ItemInstance() : ItemStackBase() {}

ItemInstance& ItemInstance::operator=(ItemInstance const& rhs) {
    ItemStackBase::operator=(rhs);
    return *this;
}
