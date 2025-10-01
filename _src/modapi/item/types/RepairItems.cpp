#include <gmlib/mod/item/types/RepairItems.h>

namespace gmlib::mod {

RepairItems::RepairItems() : mRepairAmount(0) {}

RepairItems::RepairItems(std::string_view item, float repairAmount) : mRepairAmount(repairAmount) {
    mItems.insert(std::string(item));
}

RepairItems::RepairItems(std::vector<std::string> const& items, float repairAmount) : mRepairAmount(repairAmount) {
    for (auto& item : items) {
        mItems.insert(item);
    }
}

} // namespace gmlib::mod
