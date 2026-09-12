#include <mc/world/item/ItemInstance.h>
#include <mc/world/item/ItemStackBase.h>

// Definitions for symbols not exported in LeviLamina 26.40.0 server
// (ItemInstance::ItemInstance is resolved through bedrock_runtime_api.lib)

ItemInstance& ItemInstance::operator=(ItemInstance const& rhs) {
    ItemStackBase::operator=(rhs);
    return *this;
}
