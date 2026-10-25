# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Since `26.10.0` the version number tracks the supported LeviLamina release: `26.51.0` is the build for LeviLamina 26.51.x.

<!-- The release workflow extracts the release notes from the *first* `## [x.y.z]` section of this
     file - it does not look the tag up by version. The section of the version being released
     therefore always has to be the topmost one, directly below `[Unreleased]`. -->

## [Unreleased]

### Added

- `modapi::worldgen::BiomeRegistry`: custom biomes whose behaviour is C++. A registration names the
  vanilla biome it is built from (the engine's biome components cannot be created from a native mod, so the
  template is copied), optionally claims climate cells (`BiomeClimate`, normalised to [-1, 1];
  `fullRange()` takes over a replaced biome's whole share of the world), and optionally names a surface
  builder. Registration happens in `BiomeReadyEvent`, which fires while the engine's registry is still open.
  `configureBiome` does the same for a biome that already exists - one a behaviour pack defined, or one
  another mod registered.
- `modapi::worldgen::SurfaceBuilderRegistry` with `modapi::worldgen::ICustomSurfaceBuilder` (in
  `modapi/worldgen/base/`): surface builders written in C++.
  A builder is registered once (in `SurfaceBuilderReadyEvent`) under a ModAPI allocated key - the engine's own
  keys are 0..7 - and any number of biomes can name it. The engine only hands `SurfaceBuilderData` to
  `isBestBuilder`, which cannot tell biomes apart, so ModAPI installs the builder per chunk for the biomes
  that name it instead of taking part in that resolution.
- `modapi::worldgen::BiomeProperty::mComponents`: engine biome components are installed into the biome's
  `BiomeComponentStorage` at registration, each as a `{ Bedrock::typeid_t<void> key, factory }` pair. The
  keys are handed out by the engine at start up and are not exported, so `BiomeRegistry::dumpComponentKeys()`
  prints them per biome for a given build; ids far above the vanilla range (60000 and up) are safe for a
  mod's own component data, since the engine never looks them up.
- `/locate biome <custom name>` works: the command's biome enum is built once by
  `MinecraftCommands::initCoreEnums`, so `BiomeRegistry` appends the registered names with the engine's own
  `_addEnumValuesInternal` once the server is up (`ll::service::getCommandRegistry()`, no hook needed). The biome id the command compares against is
  written by ModAPI, too - `BiomeRegistry::_register` leaves that field unset.
- The biome test ships the client side of its biomes as a `RuntimePack` resource pack (`biomes_client.json`
  with per-biome water/fog colours and one fog file each), installed while the mod loads so the pack stack a
  joining client is offered contains it.
- `src-test`: a biome that exists only in C++ (no behaviour pack) and a surface builder that is code;
  verified in game with `/locate biome` and by the builder being called during chunk generation.
- Surface builder writes reach the chunk: `PerColumnParameters::mBlocks` is the chunk's staging volume,
  indexed y-first (`index = localY + mHeight * (localZ + localX * mDepth)`, `localY = worldY - mDimensionBottom`),
  exactly as the engine's own `BlockVolumeTarget::setBlock` writes it; the engine commits the volume after the
  builder returns. The engine also builds neighbour chunks for context and drops them again, so the test marks
  one column per chunk and reads back the first marker whose chunk the level still holds
  (`surfaceBuilder.readBack`) - a discarded chunk reads as air and says nothing about the write.

- `modapi::block::BlockProperty`, built from the engine's own types (`BlockComponentGroupDescription`,
  `BlockDescription`, `BlockPermutationDescription`) and handed over with the registration:
  `registerBlock<MyBlock>("ns:name", { .mArguments = {...}, .mProperty = {...} })`. The same property drives both
  sides - its components are injected into the running block type, and serialised for a client - and it carries the
  block's states and permutations with it.
- `modapi::item::ICustomItem<T>` is a template over the engine base an item stands for (`::Item`, `::BlockItem`, ...),
  with the hooks in a non-template `ICustomItemBase`. `ICustomFoodItem<T>` and `ICustomToolItem<T>` follow the same
  shape.
- `src-test`: one item of every kind is registered and looked up by name - plain, block, food, tool and armor.

### Changed

- A block's client entry is written from its `BlockProperty`: components, tags, `menu_category` (through the
  engine's own enum to name helper), `vanilla_block_data`, block states (`properties`) and permutations.
- `_init` is no longer virtual, and the two item definition helpers are templates in the public include tree.
- A feature and a feature rule are registered with one structure, the way a block is: a
  `FeatureRegistration<>{ .mIdentifier, .mArguments }` and a `FeatureRuleRegistration<>{ .mIdentifier, .mArguments,
  .mPasses }`. Rules moved to a registry of their own (`FeatureRuleRegistry`, with `FeatureRuleReadyEvent`) and
  became a type - `ICustomFeatureRule`, whose `place(...)` yields the positions - instead of a `std::function`
  callback; `FeatureRegistry::registerFeatureRule` and its `CustomFeatureRule` are gone.
- The test's block texture is generated at runtime instead of shipping as a file.

### Removed

- The JSON/document block registration path and everything that existed for it (`registerBlockFromMemoryJson`,
  `deferBlockDocument`, `ICustomBlock`, `CustomBlockRegistry.h`, `setBlockTexture`).
- `ICustomBlockItem::getIcon` - a block item's picture is the block - and the unused `BlockRegistry` scaffolding.
- The per-item debug logging in the registries.

### Fixed

- Custom block states and permutations now reach a client: the entry carries the `properties` node the engine writes,
  which a permutation's condition needs in order to match.

## [26.51.0] - 2026-09-22

### Changed

- Adapted LeviLamina 26.51.x and BDS 26.51.1
- `HumanoidArmorItem::use` and `Actor::swing` gained a `HandSlot` parameter, so `ICustomArmorItem::use` and `ICustomToolItem::executeEvent` follow
- `Item::mAllowOffhand` became the tri-state `Item::OffhandAllowed`. `toOffhandAllowed` / `isOffhandAllowed` keep the old boolean view, and `item_properties.allow_off_hand` still carries a boolean - the 26.51 reader treats the value as one (`non-zero` is `Yes`, a missing key is `No`), so emitting the raw enum would turn `No` back into `Yes`
- Replaced `Recipes::loadRecipe` with `Recipes::_loadRecipe`, which takes the recipe id, a `RecipeType` and a scratch `Recipes::Buffers` separately instead of a `pair<string, Json::Value>`
- Followed the `ResourcePack::$ctor` (extra `I18n&`) and `ResourcePackRepository::$ctor` (extra `IMinecraftEventing`) hook signature changes, and read `ResourcePack::mPack` through `mImpl` now that 26.51 moved it into `ResourcePack::Impl`
- Every hook target was re-verified against the 26.51.1 `bedrockdata` symbol database; all 15 of them still resolve, so no hook had to be re-pointed

### Fixed

- Restored the local `ItemInstance::ItemInstance` definition. 26.51 declares it for the client only, so the prelink no longer resolves it out of the archives
- `CustomItemRegistry.h` includes `mc/common/WeakPtr.h` directly, the 26.51 include chain no longer pulls it in
- `ICustomToolItem::executeEvent` passes `HandSlot::Mainhand` to `Actor::swing`; `Item::executeEvent` and `RenderParams` still carry no hand slot

## [26.40.0] - 2026-09-12

### Changed

- Adapted LeviLamina 26.40.0 and BDS 26.40.8
- Replaced `CraftingDataPacket::prepareFromRecipes` with `CraftingDataPacketPayload::fromRecipes`, which was renamed in 26.40
- Rebuilt the smithing recipes by hand: 26.40 only declares the `SmithingTransformRecipe` / `SmithingTrimRecipe` constructors for the client, so the recipe fields are filled in directly
- Followed the `ResourcePackStack::deserialize` signature change (stream to `std::string_view`) and the `PackManifest` header move
- Dropped the now redundant local definitions of `ItemInstance::ItemInstance` and `CreativeGroupInfo::~CreativeGroupInfo`

## [26.20.0] - 2026-07-09

### Changed

- Adapted LeviLamina 26.20.0
- Relicensed the project under AGPL-3.0-or-later and added a README

## [26.10.0] - 2026-04-10

### Changed

- Adapted LeviLamina 26.10.4

### Fixed

- Fixed the signature of `FuckMultipleManifestOutput`

## [0.4.0] - 2026-01-31

### Changed

- Adapted LeviLamina 1.9.2 and BDS 1.21.132 @killcerr @n15421
- Clean tmpfix for initServer and initclient @n15421

## [0.3.1] - 2025-11-28

### Changed

- Adapted LeviLamina 1.7.6 and BDS 1.21.124 @zimuya4153
- Correct the name of the macro definition @zimuya4153

### Fixed

- Fixed the HumanoidArmorItem link issue temporarily @zimuya4153

## [0.3.0] - 2025-11-07

### Changed

- Adapted LeviLamina 1.7.x and BDS 1.21.120 @zimuya4153

## [0.2.1] - 2025-10-23

### Fixed

- Fixed the Item link issue temporarily @zimuya4153

## [0.2.0] - 2025-10-21

### Changed

- Adapted LeviLamina 1.6.x and BDS 1.21.111.1 @zimuya4153

## [0.1.1] - 2025-10-12

### Fixed

- Fixed the Chinese path and incorrect output of AddonLoader [#1] @zimuya4153

## [0.1.0] - 2025-10-04

### Added

- Complete the separation of the ModAPI library @zimuya4153

### Changed

- Refactor the worldgen api @killcerr

[#1]: http://github.com/GroupMountain/ModAPI-Release/issues/1

[Unreleased]: http://github.com/GroupMountain/ModAPI-Release/compare/v26.40.0...HEAD
[26.40.0]: http://github.com/GroupMountain/ModAPI-Release/compare/v26.20.0...v26.40.0
[26.20.0]: http://github.com/GroupMountain/ModAPI-Release/compare/v26.10.0...v26.20.0
[26.10.0]: http://github.com/GroupMountain/ModAPI-Release/compare/v0.4.0...v26.10.0
[0.4.0]: http://github.com/GroupMountain/ModAPI-Release/compare/v0.3.1...v0.4.0
[0.3.1]: http://github.com/GroupMountain/ModAPI-Release/compare/v0.3.0...v0.3.1
[0.3.0]: http://github.com/GroupMountain/ModAPI-Release/compare/v0.2.1...v0.3.0
[0.2.1]: http://github.com/GroupMountain/ModAPI-Release/compare/v0.2.0...v0.2.1
[0.2.0]: http://github.com/GroupMountain/ModAPI-Release/compare/v0.1.1...v0.2.0
[0.1.1]: http://github.com/GroupMountain/ModAPI-Release/compare/v0.1.0...v0.1.1
[0.1.0]: http://github.com/GroupMountain/ModAPI-Release/releases/tag/v0.1.0
