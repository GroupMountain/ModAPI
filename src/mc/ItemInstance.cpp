#include <mc/world/item/ItemInstance.h>
#include <mc/world/item/ItemStackBase.h>

ItemInstance::ItemInstance() : ItemStackBase() {}

ItemInstance& ItemInstance::operator=(ItemInstance const& rhs) {
    ItemStackBase::operator=(rhs);
    return *this;
}
