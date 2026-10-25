# History

Older README/CHANGELOG research notes that are useful while maintaining ModAPI but are not part of the
public contract.

- 26.51 adapted LeviLamina 26.51.x and BDS 26.51.1. Hook targets were re-verified against the 26.51.1
  `bedrockdata` symbol database.
- `HumanoidArmorItem::use` and `Actor::swing` gained a `HandSlot` parameter; `Item::mAllowOffhand` became the
  tri-state `Item::OffhandAllowed`.
- `Recipes::loadRecipe` was replaced by `Recipes::_loadRecipe`, which takes the recipe id, a `RecipeType` and
  a scratch `Recipes::Buffers`.
- `ResourcePack::$ctor` gained an `I18n&`; `ResourcePackRepository::$ctor` gained an
  `IMinecraftEventing`; `ResourcePack::mPack` moved into `ResourcePack::Impl`.
- 26.40 renamed `CraftingDataPacket::prepareFromRecipes` to `CraftingDataPacketPayload::fromRecipes`.
- The biome command enum is built once by `MinecraftCommands::initCoreEnums`; ModAPI appends custom names with
  `_addEnumValuesInternal` after the server is up.
- The creative group category must be created by `CreativeItemRegistry::createCategories()`; constructing a
  `CreativeItemGroupCategory` locally leaves its `EnableNonOwnerReferences` base unregistered.

- `ItemInstance::ItemInstance()` was exported by the 26.40 server and became client-only in 26.51; the
  local definition in `src/mc/ItemInstance.cpp` supplies the server symbol.
- `RandomValueBounds`, `EnableNonOwnerReferences`, `Recipe`, `SemVersion`, and the smithing recipe
  constructors are declared but not exported by the server SDK; their local definitions are in
  `src/mc/` and `src/modapi/`.
