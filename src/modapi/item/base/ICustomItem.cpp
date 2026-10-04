#include "modapi/item/base/ICustomItem.h"

// Everything `ICustomItem` defines is a template member, so it lives in the header: a template
// member's definition has to be visible wherever it is instantiated, and its mangled name moves with `T`.
