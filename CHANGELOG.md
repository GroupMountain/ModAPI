# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Since `26.10.0` the version number tracks the supported LeviLamina release: `26.51.0` is the build for LeviLamina 26.51.x.

<!-- The release workflow extracts the release notes from the *first* `## [x.y.z]` section of this
     file - it does not look the tag up by version. The section of the version being released
     therefore always has to be the topmost one, directly below `[Unreleased]`. -->

## [Unreleased]

- The test ships a real 16x16 PNG for its block texture (embedded, generated with Pillow, also kept at `src-test/assets/modapi_test_block.png`) instead of a placeholder file, and asserts the shipped file carries the PNG signature: a placeholder leaves a client with nothing to draw, which was a real defect spotted in review

### Added

- `modapi::item::ICustomBlockItem` (`include/modapi/item/base/ICustomBlockItem.h`): the item that places a custom block. It derives from the engine's `::BlockItem` and is registered through `ItemRegistry` like any other item (`isEntry` already accepted `std::derived_from<Entry, ::Item>`, so nothing had to be loosened), with an `initCustomItem(ICustomBlockItem&)` overload next to the existing `ICustomItem`/`ICustomArmorItem` ones. Measured: `::BlockItem` does *not* link itself to its block (`getBlockTypeForRendering` is not even overridden by it) - the link is `::Item::mBlockType`, so the class sets it in `_init`; before that an item built for a custom block reported `minecraft:air`, afterwards it reports the block. Registration timing needed no change either: block types are registered right after the level reads its definitions, which is measured to be before vanilla builds its items, so a block item finds its block; an earlier attempt to move block registration into the item pass was reverted, along with the switch to a ModAPI owned id range
- `src-test/BlockTest.cpp` overrides virtuals on both sides and asserts the effect: the block item's engine fields (max stack size 16, foil), an engine virtual (`isMusicDisk`), the item -> block link (`mBlockType`), and a block hook (`canProvideSupport`). A plain `ICustomItem`'s overrides are only *recorded*, with the reason: its engine object is replaced by the engine in a later pass (measured: the registry holds a different pointer than the one ModAPI built), so its fields read the defaults - a pre-existing ModAPI issue, out of scope here. Note also that this build has no RTTI data, so `dynamic_cast` aborts ("no RTTI data") and a `static_cast` on a pointer you own is the way to go

- Block items now send a client definition of their own: `NetworkTagBuilder` gained a `buildClientComponents(ICustomBlockItem const&)` overload and `ICustomBlockItem` gained `getIcon`, `getDisplayName` and a `buildNetworkTag` override. Without it the creative entry for a block item had no texture (the block itself rendered, so the packs were delivered - the item definition simply had no icon) and the name stayed an untranslated key. Measured after the change: the item tag carries the texture name (`blockItem.tagHasIcon`). The test also names its block item exactly like its block, as vanilla does (`minecraft:stone` is both), which the engine's block -> item resolution for pick block relies on, and its resource pack now ships `texts/en_US.lang` / `texts/zh_CN.lang`

### Fixed

- The vanilla `minecraft:stick` losing its icon on a client (`Missing icon for data-driven item 'minecraft:stick'`) was caused by this repository's own `[REGISTRY]` suite: it exercised the vanilla item setters (`setIcon`, `setDisplayName`, `addTag`, `setFireResistant`, `setRepairItem`, `getAndModifyVanillaNetworkTagInfo`) on `minecraft:stick`, which fills `mModifiedVanillaItems` and makes `VanillaItemDefinitionSendHook` replace that item's `components` in the registry packet. The suite now modifies the item it registers itself (`modapi_test:test_item`) and only reads the vanilla one. Measured: with both mods loaded the stick's definition is no longer touched, and all four suites stay green. It was *not* the block item, the runtime packs, `ItemVersion::DataDriven`, item id collisions or the creative pass - each of those was ruled out by an A/B, several of which were invalid until the manifest explained why: `ModAPI` is `"passive": true` and `test` is its only dependent, so renaming `test` away also unloaded ModAPI (LeviLamina reported "loaded 0 mods"), which is what made "disable the test mod" look like a fix
- Follow-up to that, and the reason it was so damaging: `VanillaItemDefinitionSendHook` used to *replace* an item's whole `components` node with the tag recorded by the setters, and that tag is built from `Item::buildNetworkTag()` - for a data driven vanilla item nearly empty (measured: `minecraft:stick` came out as 37 chars holding only `minecraft:hand_equipped`). So modifying such an item dropped everything it had. The hook now *merges* the recorded changes into what the engine already publishes for the item (top level, and inside `item_properties`), which keeps the data the mod did not touch - the icon of a modified item, for instance

- Re-registering an item name left the replaced item in `ItemRegistry::mItemRegistry`: the new item was appended while the old one stayed in the vector, so every pass that walks it (the client definition build, and this API's own `forEachItemInRegistry`) saw two items answering to one name. The leftover carried none of the values its mod had applied (`max stack size` 64 instead of 16), and the object handed back by the registration differed from the one the registry exposed. Registration now drops the replaced entry, and a custom item's `_init` values survive - verified server side: `plain item fields: stack = 16, glint = true`, and the registry object and the registration product are the same pointer

- `/give <custom item>` silently produced nothing: the enum entry was written as `CommandItem{id, true, false}`, but the type is a union of `{ short mVersion; bool mOverrideAux; int mId; }` over a `uint64`. The id therefore landed in `mVersion` (truncated) and `mId` was left 0. Reading the engine's own `CommandItem::createInstance` settled the rest: it only treats `mId` as the item's own id when `mVersion` is **non-zero** - with 0 it converts the value as a *legacy* id through a different table, which is why an entry for `minecraft:stick` resolved to `minecraft:oak_sign` while our item resolved to nothing. The entry is now `{1, true, id}`. The registry suite builds that value and asserts `createInstance` returns the item (`give.*`)
- Note for whoever tests this next: the item must be reached by a **real** player - a virtual one gives `No targets matched selector`; and an item whose identifier contains a dot (`modapi_test:item.test_block`) is rejected by the command parser outright, so a block item registered under that name cannot be given at all

- The definition packet a client receives was never filled in for custom items. `ModAPI` hooked
  `ItemRegistryPacket::write`, but the old build shows the packet is serialized through
  `writeWithSerializationMode` reaching `serialize<ItemRegistryPacketPayload>::write` directly - `write` is a
  different virtual, so the hook never ran. It is now on the payload constructor, which is where `mItems` is
  produced and which every sending path goes through. Reading `components` out of an entry that has none also
  threw `bad variant access` and aborted the whole merge, so the node is only read after a check
- `ICustomItem` stopped setting `mItemParseVersion = ItemVersion::DataDriven`: it was commented out during an
  experiment and left that way. That flag is what gives an item the definition machinery a client needs, and
  custom items came up blank from then on (`Item <name> requires either an icon atlas or icon texture`)
- Re-registering an item name left the replaced item in `ItemRegistry::mItemRegistry`, and
  `CreativeItemRegistry` appended a creative entry per registration, so items showed up twice in a client's
  inventory and the leftover reported default fields. Registration now drops the replaced entry, and
  `registerCreativeItem` replaces the entry for an item instead of appending another
- `/give <custom item>` silently did nothing: the item enum entry was written as `CommandItem{id, true, false}`,
  but the type is a union of `{ short mVersion; bool mOverrideAux; int mId; }`. `CommandItem::createInstance`
  only reads `mId` as the item's own id when `mVersion` is non-zero - with 0 it converts the value as a *legacy*
  id through a different table
- A block item's id has to be the block's id folded into a short (`id <= 0xFF ? id : 255 - id`), which is what
  `ItemRegistryRef::registerBlockItem<BlockItem>` hands to `BlockItem`'s constructor. Assigning an id afterwards
  left the item's own state disagreeing with `mId`, which showed up as a creative entry nothing could be done
  with
- A block whose type a mod registers in C++ is not listed by the engine at all: the engine creates items for the
  blocks it parses out of documents, so that item is queued for the creative pass instead. The queue cannot
  duplicate anything now that `registerCreativeItem` replaces

- A block item drawn as the block itself (rather than as a flat picture) needs the `minecraft:block_placer`
  component in its definition, with the block it stands for. Two details decide whether a client acts on it, and
  both come out of `PlanterItemComponent::buildNetworkTag` in the old build, which writes `canUseBlockAsIcon` from
  the component's byte +64 and `replaceBlockItem` from +65: the keys are **camelCase** in the network form (a
  *document* uses `replace_block_item`, bound by `bindType`), and `canUseBlockAsIcon` has no document key at all -
  the definition sent to a client is the only place it can be set. A `minecraft:icon` must be left out at the same
  time: `ComponentItem::getIconInfo` reads the icon component first and only falls back to the item's block when
  there is none, so writing an icon pins the item to a texture

- A block registered from C++ now reaches clients without any document. The engine builds the list a client is
  told about (`StartGamePacketPayload::mBlockProperties`) out of the block definition group, and a block whose type
  ModAPI registered itself is not in that group - so the packet's write path adds the entry it is missing. The entry
  is field for field the one the engine writes itself (read out of a live one as SNBT), which is why it works: the
  same `components`/`minecraft:material_instances`, `menu_category`, `molangVersion` and `vanilla_block_data`, with
  the block's own id. `ServerBlockProperty`'s default constructor is declared in the header but not exported, so its
  definition is written here.
- The document path is gone with it: `registerBlockFromMemoryJson`, `registerBlockFromJsonFile`, `deferBlockDocument`
  and the runtime pack they installed were removed, along with the suppression of the engine's block type and the
  bookkeeping that went with it. A block is a `::BlockType` subclass registered under an identifier, and the texture
  a client draws it with is named with `BlockRegistry::setBlockTexture` - the texture file and the resource pack
  mappings (`blocks.json`, `terrain_texture.json`) are the mod's own, exactly as they are for an addon block.
- `ICustomBlock` is gone as well: it only forwarded a constructor to `::BlockType`, so a mod derives from
  `::BlockType` directly. `ICustomFeature` moved to `include/modapi/worldgen/base/` and
  `src/modapi/worldgen/base/`, which is where every other registry keeps its interfaces.

- The definition-level part of a block entry comes out of the registration's own `BlockDescription` instead of
  defaults: `menu_category.category` through the engine's exported
  `SharedTypes::v1_21_110::ItemCategory::stringFromCreativeItemCategory`, `group` and `is_hidden_in_commands` from
  `BlockMenuCategory`, and `vanilla_block_data.block_id` / `material` from `VanillaBlockData` (the material name
  through `BlockEnum::MaterialTypeToString`). `BlockPermutationDescription`s are published as `permutations`, each
  with its `condition` (`ExpressionNode::getExpressionString`), its components and its tags - the same three nodes the
  engine writes.
- A block is registered with what to tell a client about it, in one call and in the engine's own types:
  `registerBlock<MyBlock>("mymod:my_block", { .mArguments = {...}, .mProperty = {...} })`, with `BlockProperty` made
  of `BlockComponentGroupDescription` / `BlockDescription` / `BlockPermutationDescription`. The same property drives
  both sides: its components are injected into the running block type through `initializeComponentFromCode` - the
  route vanilla uses for its own blocks - and serialised for a client through `isNetworkComponent` + `getName` +
  `buildNetworkTag`, which are the calls `generateServerBlockProperties` makes. The entry produced this way matches
  the engine's own field for field (measured against the entry the engine writes for an addon block: identical apart
  from each block's own texture name and id). No `setBlockTexture`/`setBlockProperty`: a property is handed over with
  the registration.
- The engine declares default constructors and copy operations for those definition types and does not export them,
  so `include/modapi/block/base/EngineDefaults.h` supplies the definitions inline - default constructors only, and
  copy/assignment only where the linker proved them missing. A type holding a `CompoundTag`
  (`ServerBlockProperty`) keeps the engine's exported copy: a memberwise `= default` there copies the tag shallowly
  and two objects then free the same storage, which crashed the server at `StartGamePacket` time.

### Changed

- `RuntimePack` gets a fresh uuid on every construction instead of a name derived one (and logs it): a client that already has a pack with the same uuid does not download it again, so a pack rebuilt on each server start has to offer an identity no client has seen. Packs also have to be installed while mods load - the stack clients are offered is composed when a level starts, so a pack installed later (from a command, for instance) never reaches them; the test installs its packs there now

- The test ships a real 16x16 PNG for its block texture (embedded, generated with Pillow, also kept at `src-test/assets/modapi_test_block.png`) instead of a placeholder file, and asserts the shipped file carries the PNG signature: a placeholder leaves a client with nothing to draw, which was a real defect spotted in review

### Added

- `modapi::item::ICustomBlockItem` (`include/modapi/item/base/ICustomBlockItem.h`): the item that places a custom block. It derives from the engine's `::BlockItem` and is registered through `ItemRegistry` like any other item (`isEntry` already accepted `std::derived_from<Entry, ::Item>`, so nothing had to be loosened), with an `initCustomItem(ICustomBlockItem&)` overload next to the existing `ICustomItem`/`ICustomArmorItem` ones. Measured: `::BlockItem` does *not* link itself to its block (`getBlockTypeForRendering` is not even overridden by it) - the link is `::Item::mBlockType`, so the class sets it in `_init`; before that an item built for a custom block reported `minecraft:air`, afterwards it reports the block. Registration timing needed no change either: block types are registered right after the level reads its definitions, which is measured to be before vanilla builds its items, so a block item finds its block; an earlier attempt to move block registration into the item pass was reverted, along with the switch to a ModAPI owned id range
- `src-test/BlockTest.cpp` overrides virtuals on both sides and asserts the effect: the block item's engine fields (max stack size 16, foil), an engine virtual (`isMusicDisk`), the item -> block link (`mBlockType`), and a block hook (`canProvideSupport`). A plain `ICustomItem`'s overrides are only *recorded*, with the reason: its engine object is replaced by the engine in a later pass (measured: the registry holds a different pointer than the one ModAPI built), so its fields read the defaults - a pre-existing ModAPI issue, out of scope here. Note also that this build has no RTTI data, so `dynamic_cast` aborts ("no RTTI data") and a `static_cast` on a pointer you own is the way to go

- Block items now send a client definition of their own: `NetworkTagBuilder` gained a `buildClientComponents(ICustomBlockItem const&)` overload and `ICustomBlockItem` gained `getIcon`, `getDisplayName` and a `buildNetworkTag` override. Without it the creative entry for a block item had no texture (the block itself rendered, so the packs were delivered - the item definition simply had no icon) and the name stayed an untranslated key. Measured after the change: the item tag carries the texture name (`blockItem.tagHasIcon`). The test also names its block item exactly like its block, as vanilla does (`minecraft:stone` is both), which the engine's block -> item resolution for pick block relies on, and its resource pack now ships `texts/en_US.lang` / `texts/zh_CN.lang`

### Fixed

- The vanilla `minecraft:stick` losing its icon on a client (`Missing icon for data-driven item 'minecraft:stick'`) was caused by this repository's own `[REGISTRY]` suite: it exercised the vanilla item setters (`setIcon`, `setDisplayName`, `addTag`, `setFireResistant`, `setRepairItem`, `getAndModifyVanillaNetworkTagInfo`) on `minecraft:stick`, which fills `mModifiedVanillaItems` and makes `VanillaItemDefinitionSendHook` replace that item's `components` in the registry packet. The suite now modifies the item it registers itself (`modapi_test:test_item`) and only reads the vanilla one. Measured: with both mods loaded the stick's definition is no longer touched, and all four suites stay green. It was *not* the block item, the runtime packs, `ItemVersion::DataDriven`, item id collisions or the creative pass - each of those was ruled out by an A/B, several of which were invalid until the manifest explained why: `ModAPI` is `"passive": true` and `test` is its only dependent, so renaming `test` away also unloaded ModAPI (LeviLamina reported "loaded 0 mods"), which is what made "disable the test mod" look like a fix
- Follow-up to that, and the reason it was so damaging: `VanillaItemDefinitionSendHook` used to *replace* an item's whole `components` node with the tag recorded by the setters, and that tag is built from `Item::buildNetworkTag()` - for a data driven vanilla item nearly empty (measured: `minecraft:stick` came out as 37 chars holding only `minecraft:hand_equipped`). So modifying such an item dropped everything it had. The hook now *merges* the recorded changes into what the engine already publishes for the item (top level, and inside `item_properties`), which keeps the data the mod did not touch - the icon of a modified item, for instance

- Re-registering an item name left the replaced item in `ItemRegistry::mItemRegistry`: the new item was appended while the old one stayed in the vector, so every pass that walks it (the client definition build, and this API's own `forEachItemInRegistry`) saw two items answering to one name. The leftover carried none of the values its mod had applied (`max stack size` 64 instead of 16), and the object handed back by the registration differed from the one the registry exposed. Registration now drops the replaced entry, and a custom item's `_init` values survive - verified server side: `plain item fields: stack = 16, glint = true`, and the registry object and the registration product are the same pointer

- `/give <custom item>` silently produced nothing: the enum entry was written as `CommandItem{id, true, false}`, but the type is a union of `{ short mVersion; bool mOverrideAux; int mId; }` over a `uint64`. The id therefore landed in `mVersion` (truncated) and `mId` was left 0. Reading the engine's own `CommandItem::createInstance` settled the rest: it only treats `mId` as the item's own id when `mVersion` is **non-zero** - with 0 it converts the value as a *legacy* id through a different table, which is why an entry for `minecraft:stick` resolved to `minecraft:oak_sign` while our item resolved to nothing. The entry is now `{1, true, id}`. The registry suite builds that value and asserts `createInstance` returns the item (`give.*`)
- Note for whoever tests this next: the item must be reached by a **real** player - a virtual one gives `No targets matched selector`; and an item whose identifier contains a dot (`modapi_test:item.test_block`) is rejected by the command parser outright, so a block item registered under that name cannot be given at all

- The definition packet a client receives was never filled in for custom items. `ModAPI` hooked
  `ItemRegistryPacket::write`, but the old build shows the packet is serialized through
  `writeWithSerializationMode` reaching `serialize<ItemRegistryPacketPayload>::write` directly - `write` is a
  different virtual, so the hook never ran. It is now on the payload constructor, which is where `mItems` is
  produced and which every sending path goes through. Reading `components` out of an entry that has none also
  threw `bad variant access` and aborted the whole merge, so the node is only read after a check
- `ICustomItem` stopped setting `mItemParseVersion = ItemVersion::DataDriven`: it was commented out during an
  experiment and left that way. That flag is what gives an item the definition machinery a client needs, and
  custom items came up blank from then on (`Item <name> requires either an icon atlas or icon texture`)
- Re-registering an item name left the replaced item in `ItemRegistry::mItemRegistry`, and
  `CreativeItemRegistry` appended a creative entry per registration, so items showed up twice in a client's
  inventory and the leftover reported default fields. Registration now drops the replaced entry, and
  `registerCreativeItem` replaces the entry for an item instead of appending another
- `/give <custom item>` silently did nothing: the item enum entry was written as `CommandItem{id, true, false}`,
  but the type is a union of `{ short mVersion; bool mOverrideAux; int mId; }`. `CommandItem::createInstance`
  only reads `mId` as the item's own id when `mVersion` is non-zero - with 0 it converts the value as a *legacy*
  id through a different table
- A block item's id has to be the block's id folded into a short (`id <= 0xFF ? id : 255 - id`), which is what
  `ItemRegistryRef::registerBlockItem<BlockItem>` hands to `BlockItem`'s constructor. Assigning an id afterwards
  left the item's own state disagreeing with `mId`, which showed up as a creative entry nothing could be done
  with
- A block whose type a mod registers in C++ is not listed by the engine at all: the engine creates items for the
  blocks it parses out of documents, so that item is queued for the creative pass instead. The queue cannot
  duplicate anything now that `registerCreativeItem` replaces

- A block item drawn as the block itself (rather than as a flat picture) needs the `minecraft:block_placer`
  component in its definition, with the block it stands for. Two details decide whether a client acts on it, and
  both come out of `PlanterItemComponent::buildNetworkTag` in the old build, which writes `canUseBlockAsIcon` from
  the component's byte +64 and `replaceBlockItem` from +65: the keys are **camelCase** in the network form (a
  *document* uses `replace_block_item`, bound by `bindType`), and `canUseBlockAsIcon` has no document key at all -
  the definition sent to a client is the only place it can be set. A `minecraft:icon` must be left out at the same
  time: `ComponentItem::getIconInfo` reads the icon component first and only falls back to the item's block when
  there is none, so writing an icon pins the item to a texture

- A block registered from C++ now reaches clients without any document. The engine builds the list a client is
  told about (`StartGamePacketPayload::mBlockProperties`) out of the block definition group, and a block whose type
  ModAPI registered itself is not in that group - so the packet's write path adds the entry it is missing. The entry
  is field for field the one the engine writes itself (read out of a live one as SNBT), which is why it works: the
  same `components`/`minecraft:material_instances`, `menu_category`, `molangVersion` and `vanilla_block_data`, with
  the block's own id. `ServerBlockProperty`'s default constructor is declared in the header but not exported, so its
  definition is written here.
- The document path is gone with it: `registerBlockFromMemoryJson`, `registerBlockFromJsonFile`, `deferBlockDocument`
  and the runtime pack they installed were removed, along with the suppression of the engine's block type and the
  bookkeeping that went with it. A block is a `::BlockType` subclass registered under an identifier, and the texture
  a client draws it with is named with `BlockRegistry::setBlockTexture` - the texture file and the resource pack
  mappings (`blocks.json`, `terrain_texture.json`) are the mod's own, exactly as they are for an addon block.
- `ICustomBlock` is gone as well: it only forwarded a constructor to `::BlockType`, so a mod derives from
  `::BlockType` directly. `ICustomFeature` moved to `include/modapi/worldgen/base/` and
  `src/modapi/worldgen/base/`, which is where every other registry keeps its interfaces.

- The definition-level part of a block entry comes out of the registration's own `BlockDescription` instead of
  defaults: `menu_category.category` through the engine's exported
  `SharedTypes::v1_21_110::ItemCategory::stringFromCreativeItemCategory`, `group` and `is_hidden_in_commands` from
  `BlockMenuCategory`, and `vanilla_block_data.block_id` / `material` from `VanillaBlockData` (the material name
  through `BlockEnum::MaterialTypeToString`). `BlockPermutationDescription`s are published as `permutations`, each
  with its `condition` (`ExpressionNode::getExpressionString`), its components and its tags - the same three nodes the
  engine writes.
- A block is registered with what to tell a client about it, in one call and in the engine's own types:
  `registerBlock<MyBlock>("mymod:my_block", { .mArguments = {...}, .mProperty = {...} })`, with `BlockProperty` made
  of `BlockComponentGroupDescription` / `BlockDescription` / `BlockPermutationDescription`. The same property drives
  both sides: its components are injected into the running block type through `initializeComponentFromCode` - the
  route vanilla uses for its own blocks - and serialised for a client through `isNetworkComponent` + `getName` +
  `buildNetworkTag`, which are the calls `generateServerBlockProperties` makes. The entry produced this way matches
  the engine's own field for field (measured against the entry the engine writes for an addon block: identical apart
  from each block's own texture name and id). No `setBlockTexture`/`setBlockProperty`: a property is handed over with
  the registration.
- The engine declares default constructors and copy operations for those definition types and does not export them,
  so `include/modapi/block/base/EngineDefaults.h` supplies the definitions inline - default constructors only, and
  copy/assignment only where the linker proved them missing. A type holding a `CompoundTag`
  (`ServerBlockProperty`) keeps the engine's exported copy: a memberwise `= default` there copies the tag shallowly
  and two objects then free the same storage, which crashed the server at `StartGamePacket` time.

### Changed

- Block definitions come from JSON now, and the C++ component builders are gone: `modapi::block::setMaterial`, `setGeometry` and `ICustomBlock::setupDefinition` were removed. A block's behaviour stays C++ (the mod's `ICustomBlock`, which is the `::BlockType` the engine uses), while its definition is an addon style document handed to `modapi::block::registerBlockFromJson`. That document is shipped as a behaviour pack through `RuntimePack`/`AddonsLoader`, so the engine's own parser turns it into a `BlockDefinition` while a level loads its block definitions (the `PackLoadContext` those parsers want cannot be constructed from a mod: its default constructor is not exported). The step where the engine would register its own block type for the document is hooked away for documents ModAPI knows, because registering the same identifier from C++ as well crashed the dedicated server (0xC0000005, reproduced twice: the registry keeps the existing entry and silently drops the new object). With the hook, the C++ object is the registered type and clients get the JSON definition - asserted by pointer equality and by the material component appearing in the definition. A code registration whose identifier is already defined by a pack ModAPI does not own is refused with an error rather than left inconsistent

### Added

- `modapi::addons::RuntimePack` (base `AddonsLoader`, which already existed): build any client-facing pack at runtime - arbitrary JSON and binary files, `"resources"` or `"data"` module - written into a random directory under the system temp directory and handed to the engine through `AddonsLoader::addCustomPackPath`, i.e. the same directory-pack-source path the engine loads behaviour and resource packs with, so the packs end up in the stack clients are sent. Nothing is written into the server directory or a world\'s pack list. Deliberately not specialised for blocks: a block\'s texture and definition are just files a mod adds (the test does exactly that) - the block-specific wrapper that briefly existed was removed on request
- `src-test/BlockTest.cpp` (`modapitest blocks`) installs three packs through that abstraction (a plain resource pack, a block definition pack and a texture pack), asserting the temp layout, the manifest and the copied png. What cannot be checked here is whether a real client accepts them: there is no client in this environment

- Components for custom blocks: `ICustomBlock::setupDefinition(::BlockDefinition&)` is called while a block is registered (the definition exists by then), and `modapi::block::setMaterial(definition, textureName, renderLayer, tintMethod)` adds a `minecraft:material_instances` component through the engine's own code constructor (`BlockMaterialInstancesDescription`, `MCAPI`), no `PackLoadContext` needed. Measured: material instances are *not* network components, so they do not appear in the tag `generateServerBlockProperties` builds - that tag carries the block name and the behavioural network components, while rendering data comes from the client's resource pack, exactly like an addon block. `setGeometry` is deliberately absent: `BlockGeometryDescription` derives from the template `NetworkedBlockComponentDescription<T>`, which is an empty stub in this build's headers, so it cannot be converted to a `BlockComponentDescription` (fixable by supplying the missing template in `src/mc/...`)
- `src-test/BlockTest.cpp` asserts the material reaches the block's definition (`definition.material`, `definition.material.hasTexture`)

- `modapi::block::BlockRegistry` and `modapi::block::ICustomBlock` (`include/modapi/block/`): a custom block is an engine block type (the mod derives from `ICustomBlock`, itself a `::BlockType`), registered through vanilla's own `BlockTypeRegistry::registerBlock` right after `Level::loadBlockDefinitionGroup` ran - the moment vanilla and every behaviour pack registered their definitions, and early enough that the registry still builds block states and the properties clients are told about. The id comes from `BlockDefinitionGroup::mLastBlockId` (10001 and up in this build); `BlockReadyEvent` and `DeferredRegister<BlockRegistry, ...>` work like the other registries, and the states only exist after the registry finalised, so `getDefaultBlockState` is meaningful once the server started. The registry also adds a matching `BlockDefinition` to the level's definition group, so `generateServerBlockProperties` - the list `StartGamePacket` sends - reports the block (asserted by the test); its look still comes from a client resource pack, exactly like an addon block, and whether a real client accepts it is not verifiable here
- `src-test/BlockTest.cpp` (`modapitest blocks`): registration, lookup, "the block has a default state" (the check that says it can be placed, plus a vanilla control), and a world case that places the block through `BlockHelper` in a virtual player's chunk and reads it back - skipped, and counted as such, when the environment has no loaded chunk
- `[BLOCKHELPER]` now reports every chunk dependent case as `skipped=` instead of only logging a warning, so its report says how many cases really ran

- `modapi::loot_table::LootTableRegistry` (`include/modapi/loot_table/`), which hooks `LootTables::lookupByName` - the single place vanilla resolves a loot table, so chest contents, entity drops and `LootTableReference` entries all pass through it - and serves the tables mods registered, falling back to the engine for everything else. A table can be registered from JSON text (`registerLootTableFromMemoryJson`, `registerLootTableFromJsonFile`), from an engine DOM (`registerLootTableFromJsonValue`), from an `ICustomLootTable` entry (`registerEntry`, usable through `DeferredRegister<LootTableRegistry, ...>`), or built in code (`LootTableBuilder`: item pools with weight, quality and count, plus references to other tables). Registrations made before the engine is reachable are queued and replayed on the first lookup (`hasPendingRegistrations()`), because `LootTable::deserialize` resolves items and must not run while the server is still starting; the registry publishes `LootTableReadyEvent` on that same lookup
- The loot table entry type (`ICustomLootTable`) has the same shape as the other entry types: the mod builds the engine table in `_init()` (`LootTableRegistry::buildTable` runs the engine's own `LootTable::deserialize`), and the registry only registers it; `_init()` runs on `LootTableReadyEvent`, which is the earliest moment the engine's deserializer may run at all
- Loot table documents are read against the engine's own version constant (`SharedConstants::CurrentGameSemVersion()`, `MCAPI`). Parsing a version string with `SemVersion::fromString` produced an owned packed pointer whose destructor - and the copy made on return - freed memory that was not theirs, which crashed inside `mi_free`
- `LootTableRegistry::unregisterLootTable` only drops the table the registry owns; `LootTables::mLootTables` is the engine's cache and is left alone, because erasing a cached vanilla table leaves whoever still holds that pointer dangling
- `src/mc/RandomValueBounds.cpp`: the constructors and copy assignment LeviLamina 26.51.x only declares on the server, so a loot pool - and anything else holding roll bounds - can be built from a mod; this follows the existing `src/mc/ItemInstance.cpp` pattern, and the bounds are set through `UntypedStorage::as<float>()`
- `LootTableBuilder::addEntry` takes an entry of the mod's own (`LootPoolEntry` subclass), which is the only way to get entry behaviour the engine's JSON `"type"` cannot express: the server build exposes no entry deserializer (`LootItem`/`LootTableEntry`/`LootTableReference::deserialize` are client-only) and the function/condition dispatchers (`LootItemFunctions::deserialize` is `MCAPI`, `LootItemCondition::deserialize` is `MCNAPI`) only know their built-in names. Custom functions and conditions ride along on the entry (their members are public)
- `src-test/BlockHelperTest.cpp` anchors its chunk dependent cases with a virtual player (`SimulatedPlayer::create`, `LLAPI`): creating it loads the chunks around its position, so the write cases no longer depend on the spawn chunk happening to be loaded. It is removed again through RAII (also when an assertion throws), writes no player data and leaves the shutdown clean
- `src-test/LootTableTest.cpp` rolls a table for real: `SimulatedPlayer::create` (`LLAPI`, no client needed) provides the actor, `LootTableUtils::generateRandomDeathLoot` runs the engine's own generation, and the test asserts the item a registered table describes comes out. A `Container` is not usable for this from a mod (`Container::Container()` is not exported and the class holds a `Bedrock::PubSub::Publisher`, so faking it is not safe), which is why `fillContainer`/`LootTable::fill` are out of reach
- `src-test/LootTableTest.cpp`: cases for the loot table registry - all four registration paths, the engine resolving a mod table through its own `lookupByName`, vanilla tables still resolving, unregistering, and the rejection paths

- `modapi::Registry` / `modapi::RegistryEntry` concepts and `modapi::DeferredRegister<Registry, Entry>`: a user built object that stores the constructor arguments of a custom entry type, attaches a listener for the registry ready event in its constructor, and exposes the registration product as `optional_ref<Product>` (`isRegistered()`, `isReady()`, `get()`, `operator*`, `operator->`); it also listens for `ll::event::server::ServerStoppingEvent` and detaches itself there (`detach()`), so no listener whose code lives in a mod DLL is left in the event bus while that DLL is being unloaded
- Every registry now exposes `EventType` (the ready event it publishes), `Product` / `ProductRef`, `isEntry<Entry>`, `registerEntry<Entry>(...)`, `isReady()` and `ensureEventRegistered()`
- `ItemReadyEvent`, `CreativeItemReadyEvent`, `RecipeReadyEvent`, `GameRuleReadyEvent` and `FeatureReadyEvent` are published at the vanilla registration points and can be listened to by any mod
- `CreativeItemRegistry` hooks `VanillaItems::serverInitCreativeItemsCallback`: creative groups and items are registered in the pass that builds the creative item registry, and creative items requested during the earlier item pass are queued until then
- `ModAPI::load()` registers all registry ready events; `DeferredRegister` registers them too, so a consumer that loads first still ends up with a working registry
- `src-test/`, a LeviLamina test mod (`xmake build test`) that registers one item, recipe, game rule and world generation feature through `DeferredRegister` and asserts the products, the ready events and the timing constraints on a live server, exercises the public API of every registry (item lookups and setters, creative groups/items, the recipe families, the game rule helpers, feature rules, `DeferredRegister`'s listener lifetime and concept contracts), and has a `BlockHelper` suite (handle validity, region identity, height range, out of range and "before the chunk" rejection, copy/move semantics, read/write round trips); `modapitest registry` / `modapitest blockhelper` re-run a suite in game and the reports land in `plugins/test/*-test-report.txt`. Cases that cannot run on the current server (no loaded chunk, no level) are reported as `skipped=`, not as failures
- `src/modapi/core/RegistryTemplateCheck.cpp` instantiates `DeferredRegister` for every registry at compile time, so a broken registry contract fails ModAPI's own build

- The test ships a real 16x16 PNG for its block texture (embedded, generated with Pillow, also kept at `src-test/assets/modapi_test_block.png`) instead of a placeholder file, and asserts the shipped file carries the PNG signature: a placeholder leaves a client with nothing to draw, which was a real defect spotted in review

### Added

- `modapi::item::ICustomBlockItem` (`include/modapi/item/base/ICustomBlockItem.h`): the item that places a custom block. It derives from the engine's `::BlockItem` and is registered through `ItemRegistry` like any other item (`isEntry` already accepted `std::derived_from<Entry, ::Item>`, so nothing had to be loosened), with an `initCustomItem(ICustomBlockItem&)` overload next to the existing `ICustomItem`/`ICustomArmorItem` ones. Measured: `::BlockItem` does *not* link itself to its block (`getBlockTypeForRendering` is not even overridden by it) - the link is `::Item::mBlockType`, so the class sets it in `_init`; before that an item built for a custom block reported `minecraft:air`, afterwards it reports the block. Registration timing needed no change either: block types are registered right after the level reads its definitions, which is measured to be before vanilla builds its items, so a block item finds its block; an earlier attempt to move block registration into the item pass was reverted, along with the switch to a ModAPI owned id range
- `src-test/BlockTest.cpp` overrides virtuals on both sides and asserts the effect: the block item's engine fields (max stack size 16, foil), an engine virtual (`isMusicDisk`), the item -> block link (`mBlockType`), and a block hook (`canProvideSupport`). A plain `ICustomItem`'s overrides are only *recorded*, with the reason: its engine object is replaced by the engine in a later pass (measured: the registry holds a different pointer than the one ModAPI built), so its fields read the defaults - a pre-existing ModAPI issue, out of scope here. Note also that this build has no RTTI data, so `dynamic_cast` aborts ("no RTTI data") and a `static_cast` on a pointer you own is the way to go

- Block items now send a client definition of their own: `NetworkTagBuilder` gained a `buildClientComponents(ICustomBlockItem const&)` overload and `ICustomBlockItem` gained `getIcon`, `getDisplayName` and a `buildNetworkTag` override. Without it the creative entry for a block item had no texture (the block itself rendered, so the packs were delivered - the item definition simply had no icon) and the name stayed an untranslated key. Measured after the change: the item tag carries the texture name (`blockItem.tagHasIcon`). The test also names its block item exactly like its block, as vanilla does (`minecraft:stone` is both), which the engine's block -> item resolution for pick block relies on, and its resource pack now ships `texts/en_US.lang` / `texts/zh_CN.lang`

### Fixed

- The vanilla `minecraft:stick` losing its icon on a client (`Missing icon for data-driven item 'minecraft:stick'`) was caused by this repository's own `[REGISTRY]` suite: it exercised the vanilla item setters (`setIcon`, `setDisplayName`, `addTag`, `setFireResistant`, `setRepairItem`, `getAndModifyVanillaNetworkTagInfo`) on `minecraft:stick`, which fills `mModifiedVanillaItems` and makes `VanillaItemDefinitionSendHook` replace that item's `components` in the registry packet. The suite now modifies the item it registers itself (`modapi_test:test_item`) and only reads the vanilla one. Measured: with both mods loaded the stick's definition is no longer touched, and all four suites stay green. It was *not* the block item, the runtime packs, `ItemVersion::DataDriven`, item id collisions or the creative pass - each of those was ruled out by an A/B, several of which were invalid until the manifest explained why: `ModAPI` is `"passive": true` and `test` is its only dependent, so renaming `test` away also unloaded ModAPI (LeviLamina reported "loaded 0 mods"), which is what made "disable the test mod" look like a fix
- Follow-up to that, and the reason it was so damaging: `VanillaItemDefinitionSendHook` used to *replace* an item's whole `components` node with the tag recorded by the setters, and that tag is built from `Item::buildNetworkTag()` - for a data driven vanilla item nearly empty (measured: `minecraft:stick` came out as 37 chars holding only `minecraft:hand_equipped`). So modifying such an item dropped everything it had. The hook now *merges* the recorded changes into what the engine already publishes for the item (top level, and inside `item_properties`), which keeps the data the mod did not touch - the icon of a modified item, for instance

- Re-registering an item name left the replaced item in `ItemRegistry::mItemRegistry`: the new item was appended while the old one stayed in the vector, so every pass that walks it (the client definition build, and this API's own `forEachItemInRegistry`) saw two items answering to one name. The leftover carried none of the values its mod had applied (`max stack size` 64 instead of 16), and the object handed back by the registration differed from the one the registry exposed. Registration now drops the replaced entry, and a custom item's `_init` values survive - verified server side: `plain item fields: stack = 16, glint = true`, and the registry object and the registration product are the same pointer

- `/give <custom item>` silently produced nothing: the enum entry was written as `CommandItem{id, true, false}`, but the type is a union of `{ short mVersion; bool mOverrideAux; int mId; }` over a `uint64`. The id therefore landed in `mVersion` (truncated) and `mId` was left 0. Reading the engine's own `CommandItem::createInstance` settled the rest: it only treats `mId` as the item's own id when `mVersion` is **non-zero** - with 0 it converts the value as a *legacy* id through a different table, which is why an entry for `minecraft:stick` resolved to `minecraft:oak_sign` while our item resolved to nothing. The entry is now `{1, true, id}`. The registry suite builds that value and asserts `createInstance` returns the item (`give.*`)
- Note for whoever tests this next: the item must be reached by a **real** player - a virtual one gives `No targets matched selector`; and an item whose identifier contains a dot (`modapi_test:item.test_block`) is rejected by the command parser outright, so a block item registered under that name cannot be given at all

- The definition packet a client receives was never filled in for custom items. `ModAPI` hooked
  `ItemRegistryPacket::write`, but the old build shows the packet is serialized through
  `writeWithSerializationMode` reaching `serialize<ItemRegistryPacketPayload>::write` directly - `write` is a
  different virtual, so the hook never ran. It is now on the payload constructor, which is where `mItems` is
  produced and which every sending path goes through. Reading `components` out of an entry that has none also
  threw `bad variant access` and aborted the whole merge, so the node is only read after a check
- `ICustomItem` stopped setting `mItemParseVersion = ItemVersion::DataDriven`: it was commented out during an
  experiment and left that way. That flag is what gives an item the definition machinery a client needs, and
  custom items came up blank from then on (`Item <name> requires either an icon atlas or icon texture`)
- Re-registering an item name left the replaced item in `ItemRegistry::mItemRegistry`, and
  `CreativeItemRegistry` appended a creative entry per registration, so items showed up twice in a client's
  inventory and the leftover reported default fields. Registration now drops the replaced entry, and
  `registerCreativeItem` replaces the entry for an item instead of appending another
- `/give <custom item>` silently did nothing: the item enum entry was written as `CommandItem{id, true, false}`,
  but the type is a union of `{ short mVersion; bool mOverrideAux; int mId; }`. `CommandItem::createInstance`
  only reads `mId` as the item's own id when `mVersion` is non-zero - with 0 it converts the value as a *legacy*
  id through a different table
- A block item's id has to be the block's id folded into a short (`id <= 0xFF ? id : 255 - id`), which is what
  `ItemRegistryRef::registerBlockItem<BlockItem>` hands to `BlockItem`'s constructor. Assigning an id afterwards
  left the item's own state disagreeing with `mId`, which showed up as a creative entry nothing could be done
  with
- A block whose type a mod registers in C++ is not listed by the engine at all: the engine creates items for the
  blocks it parses out of documents, so that item is queued for the creative pass instead. The queue cannot
  duplicate anything now that `registerCreativeItem` replaces

- A block item drawn as the block itself (rather than as a flat picture) needs the `minecraft:block_placer`
  component in its definition, with the block it stands for. Two details decide whether a client acts on it, and
  both come out of `PlanterItemComponent::buildNetworkTag` in the old build, which writes `canUseBlockAsIcon` from
  the component's byte +64 and `replaceBlockItem` from +65: the keys are **camelCase** in the network form (a
  *document* uses `replace_block_item`, bound by `bindType`), and `canUseBlockAsIcon` has no document key at all -
  the definition sent to a client is the only place it can be set. A `minecraft:icon` must be left out at the same
  time: `ComponentItem::getIconInfo` reads the icon component first and only falls back to the item's block when
  there is none, so writing an icon pins the item to a texture

- A block registered from C++ now reaches clients without any document. The engine builds the list a client is
  told about (`StartGamePacketPayload::mBlockProperties`) out of the block definition group, and a block whose type
  ModAPI registered itself is not in that group - so the packet's write path adds the entry it is missing. The entry
  is field for field the one the engine writes itself (read out of a live one as SNBT), which is why it works: the
  same `components`/`minecraft:material_instances`, `menu_category`, `molangVersion` and `vanilla_block_data`, with
  the block's own id. `ServerBlockProperty`'s default constructor is declared in the header but not exported, so its
  definition is written here.
- The document path is gone with it: `registerBlockFromMemoryJson`, `registerBlockFromJsonFile`, `deferBlockDocument`
  and the runtime pack they installed were removed, along with the suppression of the engine's block type and the
  bookkeeping that went with it. A block is a `::BlockType` subclass registered under an identifier, and the texture
  a client draws it with is named with `BlockRegistry::setBlockTexture` - the texture file and the resource pack
  mappings (`blocks.json`, `terrain_texture.json`) are the mod's own, exactly as they are for an addon block.
- `ICustomBlock` is gone as well: it only forwarded a constructor to `::BlockType`, so a mod derives from
  `::BlockType` directly. `ICustomFeature` moved to `include/modapi/worldgen/base/` and
  `src/modapi/worldgen/base/`, which is where every other registry keeps its interfaces.

- The definition-level part of a block entry comes out of the registration's own `BlockDescription` instead of
  defaults: `menu_category.category` through the engine's exported
  `SharedTypes::v1_21_110::ItemCategory::stringFromCreativeItemCategory`, `group` and `is_hidden_in_commands` from
  `BlockMenuCategory`, and `vanilla_block_data.block_id` / `material` from `VanillaBlockData` (the material name
  through `BlockEnum::MaterialTypeToString`). `BlockPermutationDescription`s are published as `permutations`, each
  with its `condition` (`ExpressionNode::getExpressionString`), its components and its tags - the same three nodes the
  engine writes.
- A block is registered with what to tell a client about it, in one call and in the engine's own types:
  `registerBlock<MyBlock>("mymod:my_block", { .mArguments = {...}, .mProperty = {...} })`, with `BlockProperty` made
  of `BlockComponentGroupDescription` / `BlockDescription` / `BlockPermutationDescription`. The same property drives
  both sides: its components are injected into the running block type through `initializeComponentFromCode` - the
  route vanilla uses for its own blocks - and serialised for a client through `isNetworkComponent` + `getName` +
  `buildNetworkTag`, which are the calls `generateServerBlockProperties` makes. The entry produced this way matches
  the engine's own field for field (measured against the entry the engine writes for an addon block: identical apart
  from each block's own texture name and id). No `setBlockTexture`/`setBlockProperty`: a property is handed over with
  the registration.
- The engine declares default constructors and copy operations for those definition types and does not export them,
  so `include/modapi/block/base/EngineDefaults.h` supplies the definitions inline - default constructors only, and
  copy/assignment only where the linker proved them missing. A type holding a `CompoundTag`
  (`ServerBlockProperty`) keeps the engine's exported copy: a memberwise `= default` there copies the tag shallowly
  and two objects then free the same storage, which crashed the server at `StartGamePacket` time.

### Changed

- `RuntimePack` gets a fresh uuid on every construction instead of a name derived one (and logs it): a client that already has a pack with the same uuid does not download it again, so a pack rebuilt on each server start has to offer an identity no client has seen. Packs also have to be installed while mods load - the stack clients are offered is composed when a level starts, so a pack installed later (from a command, for instance) never reaches them; the test installs its packs there now

- The test ships a real 16x16 PNG for its block texture (embedded, generated with Pillow, also kept at `src-test/assets/modapi_test_block.png`) instead of a placeholder file, and asserts the shipped file carries the PNG signature: a placeholder leaves a client with nothing to draw, which was a real defect spotted in review

### Added

- `modapi::item::ICustomBlockItem` (`include/modapi/item/base/ICustomBlockItem.h`): the item that places a custom block. It derives from the engine's `::BlockItem` and is registered through `ItemRegistry` like any other item (`isEntry` already accepted `std::derived_from<Entry, ::Item>`, so nothing had to be loosened), with an `initCustomItem(ICustomBlockItem&)` overload next to the existing `ICustomItem`/`ICustomArmorItem` ones. Measured: `::BlockItem` does *not* link itself to its block (`getBlockTypeForRendering` is not even overridden by it) - the link is `::Item::mBlockType`, so the class sets it in `_init`; before that an item built for a custom block reported `minecraft:air`, afterwards it reports the block. Registration timing needed no change either: block types are registered right after the level reads its definitions, which is measured to be before vanilla builds its items, so a block item finds its block; an earlier attempt to move block registration into the item pass was reverted, along with the switch to a ModAPI owned id range
- `src-test/BlockTest.cpp` overrides virtuals on both sides and asserts the effect: the block item's engine fields (max stack size 16, foil), an engine virtual (`isMusicDisk`), the item -> block link (`mBlockType`), and a block hook (`canProvideSupport`). A plain `ICustomItem`'s overrides are only *recorded*, with the reason: its engine object is replaced by the engine in a later pass (measured: the registry holds a different pointer than the one ModAPI built), so its fields read the defaults - a pre-existing ModAPI issue, out of scope here. Note also that this build has no RTTI data, so `dynamic_cast` aborts ("no RTTI data") and a `static_cast` on a pointer you own is the way to go

- Block items now send a client definition of their own: `NetworkTagBuilder` gained a `buildClientComponents(ICustomBlockItem const&)` overload and `ICustomBlockItem` gained `getIcon`, `getDisplayName` and a `buildNetworkTag` override. Without it the creative entry for a block item had no texture (the block itself rendered, so the packs were delivered - the item definition simply had no icon) and the name stayed an untranslated key. Measured after the change: the item tag carries the texture name (`blockItem.tagHasIcon`). The test also names its block item exactly like its block, as vanilla does (`minecraft:stone` is both), which the engine's block -> item resolution for pick block relies on, and its resource pack now ships `texts/en_US.lang` / `texts/zh_CN.lang`

### Fixed

- The vanilla `minecraft:stick` losing its icon on a client (`Missing icon for data-driven item 'minecraft:stick'`) was caused by this repository's own `[REGISTRY]` suite: it exercised the vanilla item setters (`setIcon`, `setDisplayName`, `addTag`, `setFireResistant`, `setRepairItem`, `getAndModifyVanillaNetworkTagInfo`) on `minecraft:stick`, which fills `mModifiedVanillaItems` and makes `VanillaItemDefinitionSendHook` replace that item's `components` in the registry packet. The suite now modifies the item it registers itself (`modapi_test:test_item`) and only reads the vanilla one. Measured: with both mods loaded the stick's definition is no longer touched, and all four suites stay green. It was *not* the block item, the runtime packs, `ItemVersion::DataDriven`, item id collisions or the creative pass - each of those was ruled out by an A/B, several of which were invalid until the manifest explained why: `ModAPI` is `"passive": true` and `test` is its only dependent, so renaming `test` away also unloaded ModAPI (LeviLamina reported "loaded 0 mods"), which is what made "disable the test mod" look like a fix
- Follow-up to that, and the reason it was so damaging: `VanillaItemDefinitionSendHook` used to *replace* an item's whole `components` node with the tag recorded by the setters, and that tag is built from `Item::buildNetworkTag()` - for a data driven vanilla item nearly empty (measured: `minecraft:stick` came out as 37 chars holding only `minecraft:hand_equipped`). So modifying such an item dropped everything it had. The hook now *merges* the recorded changes into what the engine already publishes for the item (top level, and inside `item_properties`), which keeps the data the mod did not touch - the icon of a modified item, for instance

- Re-registering an item name left the replaced item in `ItemRegistry::mItemRegistry`: the new item was appended while the old one stayed in the vector, so every pass that walks it (the client definition build, and this API's own `forEachItemInRegistry`) saw two items answering to one name. The leftover carried none of the values its mod had applied (`max stack size` 64 instead of 16), and the object handed back by the registration differed from the one the registry exposed. Registration now drops the replaced entry, and a custom item's `_init` values survive - verified server side: `plain item fields: stack = 16, glint = true`, and the registry object and the registration product are the same pointer

- `/give <custom item>` silently produced nothing: the enum entry was written as `CommandItem{id, true, false}`, but the type is a union of `{ short mVersion; bool mOverrideAux; int mId; }` over a `uint64`. The id therefore landed in `mVersion` (truncated) and `mId` was left 0. Reading the engine's own `CommandItem::createInstance` settled the rest: it only treats `mId` as the item's own id when `mVersion` is **non-zero** - with 0 it converts the value as a *legacy* id through a different table, which is why an entry for `minecraft:stick` resolved to `minecraft:oak_sign` while our item resolved to nothing. The entry is now `{1, true, id}`. The registry suite builds that value and asserts `createInstance` returns the item (`give.*`)
- Note for whoever tests this next: the item must be reached by a **real** player - a virtual one gives `No targets matched selector`; and an item whose identifier contains a dot (`modapi_test:item.test_block`) is rejected by the command parser outright, so a block item registered under that name cannot be given at all

- The definition packet a client receives was never filled in for custom items. `ModAPI` hooked
  `ItemRegistryPacket::write`, but the old build shows the packet is serialized through
  `writeWithSerializationMode` reaching `serialize<ItemRegistryPacketPayload>::write` directly - `write` is a
  different virtual, so the hook never ran. It is now on the payload constructor, which is where `mItems` is
  produced and which every sending path goes through. Reading `components` out of an entry that has none also
  threw `bad variant access` and aborted the whole merge, so the node is only read after a check
- `ICustomItem` stopped setting `mItemParseVersion = ItemVersion::DataDriven`: it was commented out during an
  experiment and left that way. That flag is what gives an item the definition machinery a client needs, and
  custom items came up blank from then on (`Item <name> requires either an icon atlas or icon texture`)
- Re-registering an item name left the replaced item in `ItemRegistry::mItemRegistry`, and
  `CreativeItemRegistry` appended a creative entry per registration, so items showed up twice in a client's
  inventory and the leftover reported default fields. Registration now drops the replaced entry, and
  `registerCreativeItem` replaces the entry for an item instead of appending another
- `/give <custom item>` silently did nothing: the item enum entry was written as `CommandItem{id, true, false}`,
  but the type is a union of `{ short mVersion; bool mOverrideAux; int mId; }`. `CommandItem::createInstance`
  only reads `mId` as the item's own id when `mVersion` is non-zero - with 0 it converts the value as a *legacy*
  id through a different table
- A block item's id has to be the block's id folded into a short (`id <= 0xFF ? id : 255 - id`), which is what
  `ItemRegistryRef::registerBlockItem<BlockItem>` hands to `BlockItem`'s constructor. Assigning an id afterwards
  left the item's own state disagreeing with `mId`, which showed up as a creative entry nothing could be done
  with
- A block whose type a mod registers in C++ is not listed by the engine at all: the engine creates items for the
  blocks it parses out of documents, so that item is queued for the creative pass instead. The queue cannot
  duplicate anything now that `registerCreativeItem` replaces

- A block item drawn as the block itself (rather than as a flat picture) needs the `minecraft:block_placer`
  component in its definition, with the block it stands for. Two details decide whether a client acts on it, and
  both come out of `PlanterItemComponent::buildNetworkTag` in the old build, which writes `canUseBlockAsIcon` from
  the component's byte +64 and `replaceBlockItem` from +65: the keys are **camelCase** in the network form (a
  *document* uses `replace_block_item`, bound by `bindType`), and `canUseBlockAsIcon` has no document key at all -
  the definition sent to a client is the only place it can be set. A `minecraft:icon` must be left out at the same
  time: `ComponentItem::getIconInfo` reads the icon component first and only falls back to the item's block when
  there is none, so writing an icon pins the item to a texture

- A block registered from C++ now reaches clients without any document. The engine builds the list a client is
  told about (`StartGamePacketPayload::mBlockProperties`) out of the block definition group, and a block whose type
  ModAPI registered itself is not in that group - so the packet's write path adds the entry it is missing. The entry
  is field for field the one the engine writes itself (read out of a live one as SNBT), which is why it works: the
  same `components`/`minecraft:material_instances`, `menu_category`, `molangVersion` and `vanilla_block_data`, with
  the block's own id. `ServerBlockProperty`'s default constructor is declared in the header but not exported, so its
  definition is written here.
- The document path is gone with it: `registerBlockFromMemoryJson`, `registerBlockFromJsonFile`, `deferBlockDocument`
  and the runtime pack they installed were removed, along with the suppression of the engine's block type and the
  bookkeeping that went with it. A block is a `::BlockType` subclass registered under an identifier, and the texture
  a client draws it with is named with `BlockRegistry::setBlockTexture` - the texture file and the resource pack
  mappings (`blocks.json`, `terrain_texture.json`) are the mod's own, exactly as they are for an addon block.
- `ICustomBlock` is gone as well: it only forwarded a constructor to `::BlockType`, so a mod derives from
  `::BlockType` directly. `ICustomFeature` moved to `include/modapi/worldgen/base/` and
  `src/modapi/worldgen/base/`, which is where every other registry keeps its interfaces.

- The definition-level part of a block entry comes out of the registration's own `BlockDescription` instead of
  defaults: `menu_category.category` through the engine's exported
  `SharedTypes::v1_21_110::ItemCategory::stringFromCreativeItemCategory`, `group` and `is_hidden_in_commands` from
  `BlockMenuCategory`, and `vanilla_block_data.block_id` / `material` from `VanillaBlockData` (the material name
  through `BlockEnum::MaterialTypeToString`). `BlockPermutationDescription`s are published as `permutations`, each
  with its `condition` (`ExpressionNode::getExpressionString`), its components and its tags - the same three nodes the
  engine writes.
- A block is registered with what to tell a client about it, in one call and in the engine's own types:
  `registerBlock<MyBlock>("mymod:my_block", { .mArguments = {...}, .mProperty = {...} })`, with `BlockProperty` made
  of `BlockComponentGroupDescription` / `BlockDescription` / `BlockPermutationDescription`. The same property drives
  both sides: its components are injected into the running block type through `initializeComponentFromCode` - the
  route vanilla uses for its own blocks - and serialised for a client through `isNetworkComponent` + `getName` +
  `buildNetworkTag`, which are the calls `generateServerBlockProperties` makes. The entry produced this way matches
  the engine's own field for field (measured against the entry the engine writes for an addon block: identical apart
  from each block's own texture name and id). No `setBlockTexture`/`setBlockProperty`: a property is handed over with
  the registration.
- The engine declares default constructors and copy operations for those definition types and does not export them,
  so `include/modapi/block/base/EngineDefaults.h` supplies the definitions inline - default constructors only, and
  copy/assignment only where the linker proved them missing. A type holding a `CompoundTag`
  (`ServerBlockProperty`) keeps the engine's exported copy: a memberwise `= default` there copies the tag shallowly
  and two objects then free the same storage, which crashed the server at `StartGamePacket` time.

### Changed

- **Breaking:** `BlockHelper` (the region handle a world generation feature is handed) was rebuilt: it holds the region directly instead of a heap allocated `unique_ptr` implementation, so it is a copyable/movable value - a copy or a move refers to the same region, and an empty handle (default constructed, or built from a null region) fails every operation instead of dereferencing null; `setBlock(pos, block, Layer, updateFlags)` takes `enum class BlockHelper::Layer` (`Block` / `ExtraBlock`) instead of a `uchar` and returns `bool` instead of throwing, `getBlock` returns `optional_ref<Block const>`, `get<T>()` is replaced by `getBlockSource()` / `getLevelChunk()` / `holdsBlockSource()` / `holdsLevelChunk()`, and `isValid()`, `isValidPosition(pos)`, `getMinHeight()` / `getMaxHeight()` were added
- **Breaking:** `CustomItemRegistry` → `ItemRegistry`, `CustomCreativeItemRegistry` → `CreativeItemRegistry`, `CustomRecipeRegistry` → `RecipeRegistry`, `CustomGameRuleRegistry` → `GameRuleRegistry`, `CustomFeatureRegistry` → `FeatureRegistry`, including the matching headers (and the placeholder `*Registry.h` stubs), so `#include <modapi/item/ItemRegistry.h>` replaces the old paths
- **Breaking:** registration methods return their product as `optional_ref<Product>` (e.g. `ItemRegistry::registerItem` returns `optional_ref<::Item>`, `RecipeRegistry::registerShapedRecipe` returns `optional_ref<::Recipe>`, `registerCreativeItem` returns `optional_ref<::CreativeItemEntry>`); registering before the registry opened now logs an error and returns an empty handle instead of queueing
- **Breaking:** custom game rules and features must be registered through `DeferredRegister` (or the ready events) to survive the repeated vanilla registration passes
- Recipe client sync sends `CraftingDataPacket` through `Level::forEachPlayer` + `Player::sendNetworkPacket` instead of GMLIB's raw binary stream broadcast

### Removed

- GMLIB dependency: the `gmlib_dir` option, the `GMLIB` static/shared link targets, the `gmlib` package requirement, `zstr`, and the groupmountain xmake repository
- `MODAPI_REGISTER_ITEM(S)`, `MODAPI_REGISTER_RECIPE(S)` and `MODAPI_REGISTER_GAME_RULE(S)` macros - use `DeferredRegister` instead
- The per-registry pending queues (`mPendingItems`, `mPendingModifyItems`, `mPendingRecipes`, `mPendingJsonRecipes`, `mPendingGameRules*`)

### Fixed

- `CreativeItemRegistry::registerCreativeGroup` used `at()` on the creative category map, which only vanilla fills while it builds the registry: every creative group/item registration *after* that pass threw (`invalid unordered_map<K, T> key`) and was swallowed, so the item silently never appeared - the category entry is now created on demand (through the engine's own `addChildGroup`/`addAnonymousGroup` helpers)
- `GameRuleRegistry` kept a pointer to whichever `GameRules` instance the hook last saw, which can be a temporary that is already gone; a registration afterwards wrote into freed memory (crash). It now resolves the level's own `GameRules` when there is a level
- `CustomFurnaceRecipeBase` built its input through `ItemInstance(ItemStackBase const&)`, which leaves the item id and aux value unresolved, so furnace recipes were written into the `Recipes` furnace table under the wrong key (they never matched an input)
- `ModAPI::disable()` now installs the stock log formatter before the module unloads and marks the process as shutting down, so global destructors (a `DeferredRegister`'s, a registry's hook registrar) stop touching LeviLamina while it tears down; the registry singletons are deliberately leaked for the same reason
- `ItemRegistry::_modifyItem` queued its callbacks but never ran them
- `CreativeItemRegistry::registerCreativeGroup` created the category entry itself with `try_emplace`. A `CreativeItemGroupCategory` built that way never has its `EnableNonOwnerReferences` base registered in the engine's non-owner reference list, and the engine wrote through that null list while it shut down (`0xC0000005`). Found with radare2 on `bedrock_server_mod.exe 1.26.51+0559ac5`: the crash is at RVA `0x168247A`, `mov byte ptr [rax], 0` with `RAX = 0`, right after `mov rax, [rsi + 8]` - the category's `mNonOwnerReferences` shared_ptr data pointer - and the destructor's other member offsets (`std::string` at `+0x18`, the group maps at `+0x50`/`+0x60`, the index vector at `+0x88`) match `CreativeItemGroupCategory`'s layout exactly. The registry now uses the engine's own `createCategories()` (a header declared `MCAPI` symbol, so it resolves) and its category objects
- `CreativeItemRegistry::_appendCreativeItem` now records the new entry in its group through the engine's own `CreativeGroupInfo::_addCreativeItemEntry` (a header declared `MCAPI` symbol) instead of only appending it to `mCreativeItems`, which left the group's `mItemIndexes` unmaintained
- `CreativeItemRegistry::_appendCreativeItem` no longer writes a fabricated entry into the engine's `mCreativeNetIdIndex` (`size() + 1` as the id): that map is the engine's own index, the entry already carries its network id, and the engine rebuilds the index from the entries (`CreativeItemRegistry::updateNetIdMap`)
- `FeatureRegistry` moved the stored features out on the first `VanillaFeatures::registerFeatures` pass, so every later level was configured with null features; rule based features are now rebuilt per pass
- `RecipeRegistry` and `GameRuleRegistry` dropped their queued entries after the first pass, so a reloaded world or a second `GameRules` instance lost the custom registrations
- Addon archives are extracted with the entry paths checked, so an entry can no longer escape the extraction directory
- `ensureEventRegistered()` now instantiates the registry: the vanilla hooks live in the registry's implementation, so registries nobody had touched yet (recipes, game rules, features) never installed their hooks and never became ready
- `BlockHelper`'s `LevelChunk` path only compared the chunk's high edge, so a position *before* the chunk (`origin - 1`, i.e. a negative local coordinate) was accepted and then written to a wrapped local coordinate - a block of the neighbouring chunk; the local coordinate is now required to be inside `[0, 16)`
- `scripts/include_correction.py` no longer drops the line ending of an include it rewrites (which glued the next line onto it)

## [26.51.0] - 2026-09-22

- The test ships a real 16x16 PNG for its block texture (embedded, generated with Pillow, also kept at `src-test/assets/modapi_test_block.png`) instead of a placeholder file, and asserts the shipped file carries the PNG signature: a placeholder leaves a client with nothing to draw, which was a real defect spotted in review

### Added

- `modapi::item::ICustomBlockItem` (`include/modapi/item/base/ICustomBlockItem.h`): the item that places a custom block. It derives from the engine's `::BlockItem` and is registered through `ItemRegistry` like any other item (`isEntry` already accepted `std::derived_from<Entry, ::Item>`, so nothing had to be loosened), with an `initCustomItem(ICustomBlockItem&)` overload next to the existing `ICustomItem`/`ICustomArmorItem` ones. Measured: `::BlockItem` does *not* link itself to its block (`getBlockTypeForRendering` is not even overridden by it) - the link is `::Item::mBlockType`, so the class sets it in `_init`; before that an item built for a custom block reported `minecraft:air`, afterwards it reports the block. Registration timing needed no change either: block types are registered right after the level reads its definitions, which is measured to be before vanilla builds its items, so a block item finds its block; an earlier attempt to move block registration into the item pass was reverted, along with the switch to a ModAPI owned id range
- `src-test/BlockTest.cpp` overrides virtuals on both sides and asserts the effect: the block item's engine fields (max stack size 16, foil), an engine virtual (`isMusicDisk`), the item -> block link (`mBlockType`), and a block hook (`canProvideSupport`). A plain `ICustomItem`'s overrides are only *recorded*, with the reason: its engine object is replaced by the engine in a later pass (measured: the registry holds a different pointer than the one ModAPI built), so its fields read the defaults - a pre-existing ModAPI issue, out of scope here. Note also that this build has no RTTI data, so `dynamic_cast` aborts ("no RTTI data") and a `static_cast` on a pointer you own is the way to go

- Block items now send a client definition of their own: `NetworkTagBuilder` gained a `buildClientComponents(ICustomBlockItem const&)` overload and `ICustomBlockItem` gained `getIcon`, `getDisplayName` and a `buildNetworkTag` override. Without it the creative entry for a block item had no texture (the block itself rendered, so the packs were delivered - the item definition simply had no icon) and the name stayed an untranslated key. Measured after the change: the item tag carries the texture name (`blockItem.tagHasIcon`). The test also names its block item exactly like its block, as vanilla does (`minecraft:stone` is both), which the engine's block -> item resolution for pick block relies on, and its resource pack now ships `texts/en_US.lang` / `texts/zh_CN.lang`

### Fixed

- The vanilla `minecraft:stick` losing its icon on a client (`Missing icon for data-driven item 'minecraft:stick'`) was caused by this repository's own `[REGISTRY]` suite: it exercised the vanilla item setters (`setIcon`, `setDisplayName`, `addTag`, `setFireResistant`, `setRepairItem`, `getAndModifyVanillaNetworkTagInfo`) on `minecraft:stick`, which fills `mModifiedVanillaItems` and makes `VanillaItemDefinitionSendHook` replace that item's `components` in the registry packet. The suite now modifies the item it registers itself (`modapi_test:test_item`) and only reads the vanilla one. Measured: with both mods loaded the stick's definition is no longer touched, and all four suites stay green. It was *not* the block item, the runtime packs, `ItemVersion::DataDriven`, item id collisions or the creative pass - each of those was ruled out by an A/B, several of which were invalid until the manifest explained why: `ModAPI` is `"passive": true` and `test` is its only dependent, so renaming `test` away also unloaded ModAPI (LeviLamina reported "loaded 0 mods"), which is what made "disable the test mod" look like a fix
- Follow-up to that, and the reason it was so damaging: `VanillaItemDefinitionSendHook` used to *replace* an item's whole `components` node with the tag recorded by the setters, and that tag is built from `Item::buildNetworkTag()` - for a data driven vanilla item nearly empty (measured: `minecraft:stick` came out as 37 chars holding only `minecraft:hand_equipped`). So modifying such an item dropped everything it had. The hook now *merges* the recorded changes into what the engine already publishes for the item (top level, and inside `item_properties`), which keeps the data the mod did not touch - the icon of a modified item, for instance

- Re-registering an item name left the replaced item in `ItemRegistry::mItemRegistry`: the new item was appended while the old one stayed in the vector, so every pass that walks it (the client definition build, and this API's own `forEachItemInRegistry`) saw two items answering to one name. The leftover carried none of the values its mod had applied (`max stack size` 64 instead of 16), and the object handed back by the registration differed from the one the registry exposed. Registration now drops the replaced entry, and a custom item's `_init` values survive - verified server side: `plain item fields: stack = 16, glint = true`, and the registry object and the registration product are the same pointer

- `/give <custom item>` silently produced nothing: the enum entry was written as `CommandItem{id, true, false}`, but the type is a union of `{ short mVersion; bool mOverrideAux; int mId; }` over a `uint64`. The id therefore landed in `mVersion` (truncated) and `mId` was left 0. Reading the engine's own `CommandItem::createInstance` settled the rest: it only treats `mId` as the item's own id when `mVersion` is **non-zero** - with 0 it converts the value as a *legacy* id through a different table, which is why an entry for `minecraft:stick` resolved to `minecraft:oak_sign` while our item resolved to nothing. The entry is now `{1, true, id}`. The registry suite builds that value and asserts `createInstance` returns the item (`give.*`)
- Note for whoever tests this next: the item must be reached by a **real** player - a virtual one gives `No targets matched selector`; and an item whose identifier contains a dot (`modapi_test:item.test_block`) is rejected by the command parser outright, so a block item registered under that name cannot be given at all

- The definition packet a client receives was never filled in for custom items. `ModAPI` hooked
  `ItemRegistryPacket::write`, but the old build shows the packet is serialized through
  `writeWithSerializationMode` reaching `serialize<ItemRegistryPacketPayload>::write` directly - `write` is a
  different virtual, so the hook never ran. It is now on the payload constructor, which is where `mItems` is
  produced and which every sending path goes through. Reading `components` out of an entry that has none also
  threw `bad variant access` and aborted the whole merge, so the node is only read after a check
- `ICustomItem` stopped setting `mItemParseVersion = ItemVersion::DataDriven`: it was commented out during an
  experiment and left that way. That flag is what gives an item the definition machinery a client needs, and
  custom items came up blank from then on (`Item <name> requires either an icon atlas or icon texture`)
- Re-registering an item name left the replaced item in `ItemRegistry::mItemRegistry`, and
  `CreativeItemRegistry` appended a creative entry per registration, so items showed up twice in a client's
  inventory and the leftover reported default fields. Registration now drops the replaced entry, and
  `registerCreativeItem` replaces the entry for an item instead of appending another
- `/give <custom item>` silently did nothing: the item enum entry was written as `CommandItem{id, true, false}`,
  but the type is a union of `{ short mVersion; bool mOverrideAux; int mId; }`. `CommandItem::createInstance`
  only reads `mId` as the item's own id when `mVersion` is non-zero - with 0 it converts the value as a *legacy*
  id through a different table
- A block item's id has to be the block's id folded into a short (`id <= 0xFF ? id : 255 - id`), which is what
  `ItemRegistryRef::registerBlockItem<BlockItem>` hands to `BlockItem`'s constructor. Assigning an id afterwards
  left the item's own state disagreeing with `mId`, which showed up as a creative entry nothing could be done
  with
- A block whose type a mod registers in C++ is not listed by the engine at all: the engine creates items for the
  blocks it parses out of documents, so that item is queued for the creative pass instead. The queue cannot
  duplicate anything now that `registerCreativeItem` replaces

- A block item drawn as the block itself (rather than as a flat picture) needs the `minecraft:block_placer`
  component in its definition, with the block it stands for. Two details decide whether a client acts on it, and
  both come out of `PlanterItemComponent::buildNetworkTag` in the old build, which writes `canUseBlockAsIcon` from
  the component's byte +64 and `replaceBlockItem` from +65: the keys are **camelCase** in the network form (a
  *document* uses `replace_block_item`, bound by `bindType`), and `canUseBlockAsIcon` has no document key at all -
  the definition sent to a client is the only place it can be set. A `minecraft:icon` must be left out at the same
  time: `ComponentItem::getIconInfo` reads the icon component first and only falls back to the item's block when
  there is none, so writing an icon pins the item to a texture

- A block registered from C++ now reaches clients without any document. The engine builds the list a client is
  told about (`StartGamePacketPayload::mBlockProperties`) out of the block definition group, and a block whose type
  ModAPI registered itself is not in that group - so the packet's write path adds the entry it is missing. The entry
  is field for field the one the engine writes itself (read out of a live one as SNBT), which is why it works: the
  same `components`/`minecraft:material_instances`, `menu_category`, `molangVersion` and `vanilla_block_data`, with
  the block's own id. `ServerBlockProperty`'s default constructor is declared in the header but not exported, so its
  definition is written here.
- The document path is gone with it: `registerBlockFromMemoryJson`, `registerBlockFromJsonFile`, `deferBlockDocument`
  and the runtime pack they installed were removed, along with the suppression of the engine's block type and the
  bookkeeping that went with it. A block is a `::BlockType` subclass registered under an identifier, and the texture
  a client draws it with is named with `BlockRegistry::setBlockTexture` - the texture file and the resource pack
  mappings (`blocks.json`, `terrain_texture.json`) are the mod's own, exactly as they are for an addon block.
- `ICustomBlock` is gone as well: it only forwarded a constructor to `::BlockType`, so a mod derives from
  `::BlockType` directly. `ICustomFeature` moved to `include/modapi/worldgen/base/` and
  `src/modapi/worldgen/base/`, which is where every other registry keeps its interfaces.

- The definition-level part of a block entry comes out of the registration's own `BlockDescription` instead of
  defaults: `menu_category.category` through the engine's exported
  `SharedTypes::v1_21_110::ItemCategory::stringFromCreativeItemCategory`, `group` and `is_hidden_in_commands` from
  `BlockMenuCategory`, and `vanilla_block_data.block_id` / `material` from `VanillaBlockData` (the material name
  through `BlockEnum::MaterialTypeToString`). `BlockPermutationDescription`s are published as `permutations`, each
  with its `condition` (`ExpressionNode::getExpressionString`), its components and its tags - the same three nodes the
  engine writes.
- A block is registered with what to tell a client about it, in one call and in the engine's own types:
  `registerBlock<MyBlock>("mymod:my_block", { .mArguments = {...}, .mProperty = {...} })`, with `BlockProperty` made
  of `BlockComponentGroupDescription` / `BlockDescription` / `BlockPermutationDescription`. The same property drives
  both sides: its components are injected into the running block type through `initializeComponentFromCode` - the
  route vanilla uses for its own blocks - and serialised for a client through `isNetworkComponent` + `getName` +
  `buildNetworkTag`, which are the calls `generateServerBlockProperties` makes. The entry produced this way matches
  the engine's own field for field (measured against the entry the engine writes for an addon block: identical apart
  from each block's own texture name and id). No `setBlockTexture`/`setBlockProperty`: a property is handed over with
  the registration.
- The engine declares default constructors and copy operations for those definition types and does not export them,
  so `include/modapi/block/base/EngineDefaults.h` supplies the definitions inline - default constructors only, and
  copy/assignment only where the linker proved them missing. A type holding a `CompoundTag`
  (`ServerBlockProperty`) keeps the engine's exported copy: a memberwise `= default` there copies the tag shallowly
  and two objects then free the same storage, which crashed the server at `StartGamePacket` time.

### Changed

- `RuntimePack` gets a fresh uuid on every construction instead of a name derived one (and logs it): a client that already has a pack with the same uuid does not download it again, so a pack rebuilt on each server start has to offer an identity no client has seen. Packs also have to be installed while mods load - the stack clients are offered is composed when a level starts, so a pack installed later (from a command, for instance) never reaches them; the test installs its packs there now

- The test ships a real 16x16 PNG for its block texture (embedded, generated with Pillow, also kept at `src-test/assets/modapi_test_block.png`) instead of a placeholder file, and asserts the shipped file carries the PNG signature: a placeholder leaves a client with nothing to draw, which was a real defect spotted in review

### Added

- `modapi::item::ICustomBlockItem` (`include/modapi/item/base/ICustomBlockItem.h`): the item that places a custom block. It derives from the engine's `::BlockItem` and is registered through `ItemRegistry` like any other item (`isEntry` already accepted `std::derived_from<Entry, ::Item>`, so nothing had to be loosened), with an `initCustomItem(ICustomBlockItem&)` overload next to the existing `ICustomItem`/`ICustomArmorItem` ones. Measured: `::BlockItem` does *not* link itself to its block (`getBlockTypeForRendering` is not even overridden by it) - the link is `::Item::mBlockType`, so the class sets it in `_init`; before that an item built for a custom block reported `minecraft:air`, afterwards it reports the block. Registration timing needed no change either: block types are registered right after the level reads its definitions, which is measured to be before vanilla builds its items, so a block item finds its block; an earlier attempt to move block registration into the item pass was reverted, along with the switch to a ModAPI owned id range
- `src-test/BlockTest.cpp` overrides virtuals on both sides and asserts the effect: the block item's engine fields (max stack size 16, foil), an engine virtual (`isMusicDisk`), the item -> block link (`mBlockType`), and a block hook (`canProvideSupport`). A plain `ICustomItem`'s overrides are only *recorded*, with the reason: its engine object is replaced by the engine in a later pass (measured: the registry holds a different pointer than the one ModAPI built), so its fields read the defaults - a pre-existing ModAPI issue, out of scope here. Note also that this build has no RTTI data, so `dynamic_cast` aborts ("no RTTI data") and a `static_cast` on a pointer you own is the way to go

- Block items now send a client definition of their own: `NetworkTagBuilder` gained a `buildClientComponents(ICustomBlockItem const&)` overload and `ICustomBlockItem` gained `getIcon`, `getDisplayName` and a `buildNetworkTag` override. Without it the creative entry for a block item had no texture (the block itself rendered, so the packs were delivered - the item definition simply had no icon) and the name stayed an untranslated key. Measured after the change: the item tag carries the texture name (`blockItem.tagHasIcon`). The test also names its block item exactly like its block, as vanilla does (`minecraft:stone` is both), which the engine's block -> item resolution for pick block relies on, and its resource pack now ships `texts/en_US.lang` / `texts/zh_CN.lang`

### Fixed

- The vanilla `minecraft:stick` losing its icon on a client (`Missing icon for data-driven item 'minecraft:stick'`) was caused by this repository's own `[REGISTRY]` suite: it exercised the vanilla item setters (`setIcon`, `setDisplayName`, `addTag`, `setFireResistant`, `setRepairItem`, `getAndModifyVanillaNetworkTagInfo`) on `minecraft:stick`, which fills `mModifiedVanillaItems` and makes `VanillaItemDefinitionSendHook` replace that item's `components` in the registry packet. The suite now modifies the item it registers itself (`modapi_test:test_item`) and only reads the vanilla one. Measured: with both mods loaded the stick's definition is no longer touched, and all four suites stay green. It was *not* the block item, the runtime packs, `ItemVersion::DataDriven`, item id collisions or the creative pass - each of those was ruled out by an A/B, several of which were invalid until the manifest explained why: `ModAPI` is `"passive": true` and `test` is its only dependent, so renaming `test` away also unloaded ModAPI (LeviLamina reported "loaded 0 mods"), which is what made "disable the test mod" look like a fix
- Follow-up to that, and the reason it was so damaging: `VanillaItemDefinitionSendHook` used to *replace* an item's whole `components` node with the tag recorded by the setters, and that tag is built from `Item::buildNetworkTag()` - for a data driven vanilla item nearly empty (measured: `minecraft:stick` came out as 37 chars holding only `minecraft:hand_equipped`). So modifying such an item dropped everything it had. The hook now *merges* the recorded changes into what the engine already publishes for the item (top level, and inside `item_properties`), which keeps the data the mod did not touch - the icon of a modified item, for instance

- Re-registering an item name left the replaced item in `ItemRegistry::mItemRegistry`: the new item was appended while the old one stayed in the vector, so every pass that walks it (the client definition build, and this API's own `forEachItemInRegistry`) saw two items answering to one name. The leftover carried none of the values its mod had applied (`max stack size` 64 instead of 16), and the object handed back by the registration differed from the one the registry exposed. Registration now drops the replaced entry, and a custom item's `_init` values survive - verified server side: `plain item fields: stack = 16, glint = true`, and the registry object and the registration product are the same pointer

- `/give <custom item>` silently produced nothing: the enum entry was written as `CommandItem{id, true, false}`, but the type is a union of `{ short mVersion; bool mOverrideAux; int mId; }` over a `uint64`. The id therefore landed in `mVersion` (truncated) and `mId` was left 0. Reading the engine's own `CommandItem::createInstance` settled the rest: it only treats `mId` as the item's own id when `mVersion` is **non-zero** - with 0 it converts the value as a *legacy* id through a different table, which is why an entry for `minecraft:stick` resolved to `minecraft:oak_sign` while our item resolved to nothing. The entry is now `{1, true, id}`. The registry suite builds that value and asserts `createInstance` returns the item (`give.*`)
- Note for whoever tests this next: the item must be reached by a **real** player - a virtual one gives `No targets matched selector`; and an item whose identifier contains a dot (`modapi_test:item.test_block`) is rejected by the command parser outright, so a block item registered under that name cannot be given at all

- The definition packet a client receives was never filled in for custom items. `ModAPI` hooked
  `ItemRegistryPacket::write`, but the old build shows the packet is serialized through
  `writeWithSerializationMode` reaching `serialize<ItemRegistryPacketPayload>::write` directly - `write` is a
  different virtual, so the hook never ran. It is now on the payload constructor, which is where `mItems` is
  produced and which every sending path goes through. Reading `components` out of an entry that has none also
  threw `bad variant access` and aborted the whole merge, so the node is only read after a check
- `ICustomItem` stopped setting `mItemParseVersion = ItemVersion::DataDriven`: it was commented out during an
  experiment and left that way. That flag is what gives an item the definition machinery a client needs, and
  custom items came up blank from then on (`Item <name> requires either an icon atlas or icon texture`)
- Re-registering an item name left the replaced item in `ItemRegistry::mItemRegistry`, and
  `CreativeItemRegistry` appended a creative entry per registration, so items showed up twice in a client's
  inventory and the leftover reported default fields. Registration now drops the replaced entry, and
  `registerCreativeItem` replaces the entry for an item instead of appending another
- `/give <custom item>` silently did nothing: the item enum entry was written as `CommandItem{id, true, false}`,
  but the type is a union of `{ short mVersion; bool mOverrideAux; int mId; }`. `CommandItem::createInstance`
  only reads `mId` as the item's own id when `mVersion` is non-zero - with 0 it converts the value as a *legacy*
  id through a different table
- A block item's id has to be the block's id folded into a short (`id <= 0xFF ? id : 255 - id`), which is what
  `ItemRegistryRef::registerBlockItem<BlockItem>` hands to `BlockItem`'s constructor. Assigning an id afterwards
  left the item's own state disagreeing with `mId`, which showed up as a creative entry nothing could be done
  with
- A block whose type a mod registers in C++ is not listed by the engine at all: the engine creates items for the
  blocks it parses out of documents, so that item is queued for the creative pass instead. The queue cannot
  duplicate anything now that `registerCreativeItem` replaces

- A block item drawn as the block itself (rather than as a flat picture) needs the `minecraft:block_placer`
  component in its definition, with the block it stands for. Two details decide whether a client acts on it, and
  both come out of `PlanterItemComponent::buildNetworkTag` in the old build, which writes `canUseBlockAsIcon` from
  the component's byte +64 and `replaceBlockItem` from +65: the keys are **camelCase** in the network form (a
  *document* uses `replace_block_item`, bound by `bindType`), and `canUseBlockAsIcon` has no document key at all -
  the definition sent to a client is the only place it can be set. A `minecraft:icon` must be left out at the same
  time: `ComponentItem::getIconInfo` reads the icon component first and only falls back to the item's block when
  there is none, so writing an icon pins the item to a texture

- A block registered from C++ now reaches clients without any document. The engine builds the list a client is
  told about (`StartGamePacketPayload::mBlockProperties`) out of the block definition group, and a block whose type
  ModAPI registered itself is not in that group - so the packet's write path adds the entry it is missing. The entry
  is field for field the one the engine writes itself (read out of a live one as SNBT), which is why it works: the
  same `components`/`minecraft:material_instances`, `menu_category`, `molangVersion` and `vanilla_block_data`, with
  the block's own id. `ServerBlockProperty`'s default constructor is declared in the header but not exported, so its
  definition is written here.
- The document path is gone with it: `registerBlockFromMemoryJson`, `registerBlockFromJsonFile`, `deferBlockDocument`
  and the runtime pack they installed were removed, along with the suppression of the engine's block type and the
  bookkeeping that went with it. A block is a `::BlockType` subclass registered under an identifier, and the texture
  a client draws it with is named with `BlockRegistry::setBlockTexture` - the texture file and the resource pack
  mappings (`blocks.json`, `terrain_texture.json`) are the mod's own, exactly as they are for an addon block.
- `ICustomBlock` is gone as well: it only forwarded a constructor to `::BlockType`, so a mod derives from
  `::BlockType` directly. `ICustomFeature` moved to `include/modapi/worldgen/base/` and
  `src/modapi/worldgen/base/`, which is where every other registry keeps its interfaces.

- The definition-level part of a block entry comes out of the registration's own `BlockDescription` instead of
  defaults: `menu_category.category` through the engine's exported
  `SharedTypes::v1_21_110::ItemCategory::stringFromCreativeItemCategory`, `group` and `is_hidden_in_commands` from
  `BlockMenuCategory`, and `vanilla_block_data.block_id` / `material` from `VanillaBlockData` (the material name
  through `BlockEnum::MaterialTypeToString`). `BlockPermutationDescription`s are published as `permutations`, each
  with its `condition` (`ExpressionNode::getExpressionString`), its components and its tags - the same three nodes the
  engine writes.
- A block is registered with what to tell a client about it, in one call and in the engine's own types:
  `registerBlock<MyBlock>("mymod:my_block", { .mArguments = {...}, .mProperty = {...} })`, with `BlockProperty` made
  of `BlockComponentGroupDescription` / `BlockDescription` / `BlockPermutationDescription`. The same property drives
  both sides: its components are injected into the running block type through `initializeComponentFromCode` - the
  route vanilla uses for its own blocks - and serialised for a client through `isNetworkComponent` + `getName` +
  `buildNetworkTag`, which are the calls `generateServerBlockProperties` makes. The entry produced this way matches
  the engine's own field for field (measured against the entry the engine writes for an addon block: identical apart
  from each block's own texture name and id). No `setBlockTexture`/`setBlockProperty`: a property is handed over with
  the registration.
- The engine declares default constructors and copy operations for those definition types and does not export them,
  so `include/modapi/block/base/EngineDefaults.h` supplies the definitions inline - default constructors only, and
  copy/assignment only where the linker proved them missing. A type holding a `CompoundTag`
  (`ServerBlockProperty`) keeps the engine's exported copy: a memberwise `= default` there copies the tag shallowly
  and two objects then free the same storage, which crashed the server at `StartGamePacket` time.

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

- The test ships a real 16x16 PNG for its block texture (embedded, generated with Pillow, also kept at `src-test/assets/modapi_test_block.png`) instead of a placeholder file, and asserts the shipped file carries the PNG signature: a placeholder leaves a client with nothing to draw, which was a real defect spotted in review

### Added

- `modapi::item::ICustomBlockItem` (`include/modapi/item/base/ICustomBlockItem.h`): the item that places a custom block. It derives from the engine's `::BlockItem` and is registered through `ItemRegistry` like any other item (`isEntry` already accepted `std::derived_from<Entry, ::Item>`, so nothing had to be loosened), with an `initCustomItem(ICustomBlockItem&)` overload next to the existing `ICustomItem`/`ICustomArmorItem` ones. Measured: `::BlockItem` does *not* link itself to its block (`getBlockTypeForRendering` is not even overridden by it) - the link is `::Item::mBlockType`, so the class sets it in `_init`; before that an item built for a custom block reported `minecraft:air`, afterwards it reports the block. Registration timing needed no change either: block types are registered right after the level reads its definitions, which is measured to be before vanilla builds its items, so a block item finds its block; an earlier attempt to move block registration into the item pass was reverted, along with the switch to a ModAPI owned id range
- `src-test/BlockTest.cpp` overrides virtuals on both sides and asserts the effect: the block item's engine fields (max stack size 16, foil), an engine virtual (`isMusicDisk`), the item -> block link (`mBlockType`), and a block hook (`canProvideSupport`). A plain `ICustomItem`'s overrides are only *recorded*, with the reason: its engine object is replaced by the engine in a later pass (measured: the registry holds a different pointer than the one ModAPI built), so its fields read the defaults - a pre-existing ModAPI issue, out of scope here. Note also that this build has no RTTI data, so `dynamic_cast` aborts ("no RTTI data") and a `static_cast` on a pointer you own is the way to go

- Block items now send a client definition of their own: `NetworkTagBuilder` gained a `buildClientComponents(ICustomBlockItem const&)` overload and `ICustomBlockItem` gained `getIcon`, `getDisplayName` and a `buildNetworkTag` override. Without it the creative entry for a block item had no texture (the block itself rendered, so the packs were delivered - the item definition simply had no icon) and the name stayed an untranslated key. Measured after the change: the item tag carries the texture name (`blockItem.tagHasIcon`). The test also names its block item exactly like its block, as vanilla does (`minecraft:stone` is both), which the engine's block -> item resolution for pick block relies on, and its resource pack now ships `texts/en_US.lang` / `texts/zh_CN.lang`

### Fixed

- The vanilla `minecraft:stick` losing its icon on a client (`Missing icon for data-driven item 'minecraft:stick'`) was caused by this repository's own `[REGISTRY]` suite: it exercised the vanilla item setters (`setIcon`, `setDisplayName`, `addTag`, `setFireResistant`, `setRepairItem`, `getAndModifyVanillaNetworkTagInfo`) on `minecraft:stick`, which fills `mModifiedVanillaItems` and makes `VanillaItemDefinitionSendHook` replace that item's `components` in the registry packet. The suite now modifies the item it registers itself (`modapi_test:test_item`) and only reads the vanilla one. Measured: with both mods loaded the stick's definition is no longer touched, and all four suites stay green. It was *not* the block item, the runtime packs, `ItemVersion::DataDriven`, item id collisions or the creative pass - each of those was ruled out by an A/B, several of which were invalid until the manifest explained why: `ModAPI` is `"passive": true` and `test` is its only dependent, so renaming `test` away also unloaded ModAPI (LeviLamina reported "loaded 0 mods"), which is what made "disable the test mod" look like a fix
- Follow-up to that, and the reason it was so damaging: `VanillaItemDefinitionSendHook` used to *replace* an item's whole `components` node with the tag recorded by the setters, and that tag is built from `Item::buildNetworkTag()` - for a data driven vanilla item nearly empty (measured: `minecraft:stick` came out as 37 chars holding only `minecraft:hand_equipped`). So modifying such an item dropped everything it had. The hook now *merges* the recorded changes into what the engine already publishes for the item (top level, and inside `item_properties`), which keeps the data the mod did not touch - the icon of a modified item, for instance

- Re-registering an item name left the replaced item in `ItemRegistry::mItemRegistry`: the new item was appended while the old one stayed in the vector, so every pass that walks it (the client definition build, and this API's own `forEachItemInRegistry`) saw two items answering to one name. The leftover carried none of the values its mod had applied (`max stack size` 64 instead of 16), and the object handed back by the registration differed from the one the registry exposed. Registration now drops the replaced entry, and a custom item's `_init` values survive - verified server side: `plain item fields: stack = 16, glint = true`, and the registry object and the registration product are the same pointer

- `/give <custom item>` silently produced nothing: the enum entry was written as `CommandItem{id, true, false}`, but the type is a union of `{ short mVersion; bool mOverrideAux; int mId; }` over a `uint64`. The id therefore landed in `mVersion` (truncated) and `mId` was left 0. Reading the engine's own `CommandItem::createInstance` settled the rest: it only treats `mId` as the item's own id when `mVersion` is **non-zero** - with 0 it converts the value as a *legacy* id through a different table, which is why an entry for `minecraft:stick` resolved to `minecraft:oak_sign` while our item resolved to nothing. The entry is now `{1, true, id}`. The registry suite builds that value and asserts `createInstance` returns the item (`give.*`)
- Note for whoever tests this next: the item must be reached by a **real** player - a virtual one gives `No targets matched selector`; and an item whose identifier contains a dot (`modapi_test:item.test_block`) is rejected by the command parser outright, so a block item registered under that name cannot be given at all

- The definition packet a client receives was never filled in for custom items. `ModAPI` hooked
  `ItemRegistryPacket::write`, but the old build shows the packet is serialized through
  `writeWithSerializationMode` reaching `serialize<ItemRegistryPacketPayload>::write` directly - `write` is a
  different virtual, so the hook never ran. It is now on the payload constructor, which is where `mItems` is
  produced and which every sending path goes through. Reading `components` out of an entry that has none also
  threw `bad variant access` and aborted the whole merge, so the node is only read after a check
- `ICustomItem` stopped setting `mItemParseVersion = ItemVersion::DataDriven`: it was commented out during an
  experiment and left that way. That flag is what gives an item the definition machinery a client needs, and
  custom items came up blank from then on (`Item <name> requires either an icon atlas or icon texture`)
- Re-registering an item name left the replaced item in `ItemRegistry::mItemRegistry`, and
  `CreativeItemRegistry` appended a creative entry per registration, so items showed up twice in a client's
  inventory and the leftover reported default fields. Registration now drops the replaced entry, and
  `registerCreativeItem` replaces the entry for an item instead of appending another
- `/give <custom item>` silently did nothing: the item enum entry was written as `CommandItem{id, true, false}`,
  but the type is a union of `{ short mVersion; bool mOverrideAux; int mId; }`. `CommandItem::createInstance`
  only reads `mId` as the item's own id when `mVersion` is non-zero - with 0 it converts the value as a *legacy*
  id through a different table
- A block item's id has to be the block's id folded into a short (`id <= 0xFF ? id : 255 - id`), which is what
  `ItemRegistryRef::registerBlockItem<BlockItem>` hands to `BlockItem`'s constructor. Assigning an id afterwards
  left the item's own state disagreeing with `mId`, which showed up as a creative entry nothing could be done
  with
- A block whose type a mod registers in C++ is not listed by the engine at all: the engine creates items for the
  blocks it parses out of documents, so that item is queued for the creative pass instead. The queue cannot
  duplicate anything now that `registerCreativeItem` replaces

- A block item drawn as the block itself (rather than as a flat picture) needs the `minecraft:block_placer`
  component in its definition, with the block it stands for. Two details decide whether a client acts on it, and
  both come out of `PlanterItemComponent::buildNetworkTag` in the old build, which writes `canUseBlockAsIcon` from
  the component's byte +64 and `replaceBlockItem` from +65: the keys are **camelCase** in the network form (a
  *document* uses `replace_block_item`, bound by `bindType`), and `canUseBlockAsIcon` has no document key at all -
  the definition sent to a client is the only place it can be set. A `minecraft:icon` must be left out at the same
  time: `ComponentItem::getIconInfo` reads the icon component first and only falls back to the item's block when
  there is none, so writing an icon pins the item to a texture

- A block registered from C++ now reaches clients without any document. The engine builds the list a client is
  told about (`StartGamePacketPayload::mBlockProperties`) out of the block definition group, and a block whose type
  ModAPI registered itself is not in that group - so the packet's write path adds the entry it is missing. The entry
  is field for field the one the engine writes itself (read out of a live one as SNBT), which is why it works: the
  same `components`/`minecraft:material_instances`, `menu_category`, `molangVersion` and `vanilla_block_data`, with
  the block's own id. `ServerBlockProperty`'s default constructor is declared in the header but not exported, so its
  definition is written here.
- The document path is gone with it: `registerBlockFromMemoryJson`, `registerBlockFromJsonFile`, `deferBlockDocument`
  and the runtime pack they installed were removed, along with the suppression of the engine's block type and the
  bookkeeping that went with it. A block is a `::BlockType` subclass registered under an identifier, and the texture
  a client draws it with is named with `BlockRegistry::setBlockTexture` - the texture file and the resource pack
  mappings (`blocks.json`, `terrain_texture.json`) are the mod's own, exactly as they are for an addon block.
- `ICustomBlock` is gone as well: it only forwarded a constructor to `::BlockType`, so a mod derives from
  `::BlockType` directly. `ICustomFeature` moved to `include/modapi/worldgen/base/` and
  `src/modapi/worldgen/base/`, which is where every other registry keeps its interfaces.

- The definition-level part of a block entry comes out of the registration's own `BlockDescription` instead of
  defaults: `menu_category.category` through the engine's exported
  `SharedTypes::v1_21_110::ItemCategory::stringFromCreativeItemCategory`, `group` and `is_hidden_in_commands` from
  `BlockMenuCategory`, and `vanilla_block_data.block_id` / `material` from `VanillaBlockData` (the material name
  through `BlockEnum::MaterialTypeToString`). `BlockPermutationDescription`s are published as `permutations`, each
  with its `condition` (`ExpressionNode::getExpressionString`), its components and its tags - the same three nodes the
  engine writes.
- A block is registered with what to tell a client about it, in one call and in the engine's own types:
  `registerBlock<MyBlock>("mymod:my_block", { .mArguments = {...}, .mProperty = {...} })`, with `BlockProperty` made
  of `BlockComponentGroupDescription` / `BlockDescription` / `BlockPermutationDescription`. The same property drives
  both sides: its components are injected into the running block type through `initializeComponentFromCode` - the
  route vanilla uses for its own blocks - and serialised for a client through `isNetworkComponent` + `getName` +
  `buildNetworkTag`, which are the calls `generateServerBlockProperties` makes. The entry produced this way matches
  the engine's own field for field (measured against the entry the engine writes for an addon block: identical apart
  from each block's own texture name and id). No `setBlockTexture`/`setBlockProperty`: a property is handed over with
  the registration.
- The engine declares default constructors and copy operations for those definition types and does not export them,
  so `include/modapi/block/base/EngineDefaults.h` supplies the definitions inline - default constructors only, and
  copy/assignment only where the linker proved them missing. A type holding a `CompoundTag`
  (`ServerBlockProperty`) keeps the engine's exported copy: a memberwise `= default` there copies the tag shallowly
  and two objects then free the same storage, which crashed the server at `StartGamePacket` time.

### Changed

- `RuntimePack` gets a fresh uuid on every construction instead of a name derived one (and logs it): a client that already has a pack with the same uuid does not download it again, so a pack rebuilt on each server start has to offer an identity no client has seen. Packs also have to be installed while mods load - the stack clients are offered is composed when a level starts, so a pack installed later (from a command, for instance) never reaches them; the test installs its packs there now

- The test ships a real 16x16 PNG for its block texture (embedded, generated with Pillow, also kept at `src-test/assets/modapi_test_block.png`) instead of a placeholder file, and asserts the shipped file carries the PNG signature: a placeholder leaves a client with nothing to draw, which was a real defect spotted in review

### Added

- `modapi::item::ICustomBlockItem` (`include/modapi/item/base/ICustomBlockItem.h`): the item that places a custom block. It derives from the engine's `::BlockItem` and is registered through `ItemRegistry` like any other item (`isEntry` already accepted `std::derived_from<Entry, ::Item>`, so nothing had to be loosened), with an `initCustomItem(ICustomBlockItem&)` overload next to the existing `ICustomItem`/`ICustomArmorItem` ones. Measured: `::BlockItem` does *not* link itself to its block (`getBlockTypeForRendering` is not even overridden by it) - the link is `::Item::mBlockType`, so the class sets it in `_init`; before that an item built for a custom block reported `minecraft:air`, afterwards it reports the block. Registration timing needed no change either: block types are registered right after the level reads its definitions, which is measured to be before vanilla builds its items, so a block item finds its block; an earlier attempt to move block registration into the item pass was reverted, along with the switch to a ModAPI owned id range
- `src-test/BlockTest.cpp` overrides virtuals on both sides and asserts the effect: the block item's engine fields (max stack size 16, foil), an engine virtual (`isMusicDisk`), the item -> block link (`mBlockType`), and a block hook (`canProvideSupport`). A plain `ICustomItem`'s overrides are only *recorded*, with the reason: its engine object is replaced by the engine in a later pass (measured: the registry holds a different pointer than the one ModAPI built), so its fields read the defaults - a pre-existing ModAPI issue, out of scope here. Note also that this build has no RTTI data, so `dynamic_cast` aborts ("no RTTI data") and a `static_cast` on a pointer you own is the way to go

- Block items now send a client definition of their own: `NetworkTagBuilder` gained a `buildClientComponents(ICustomBlockItem const&)` overload and `ICustomBlockItem` gained `getIcon`, `getDisplayName` and a `buildNetworkTag` override. Without it the creative entry for a block item had no texture (the block itself rendered, so the packs were delivered - the item definition simply had no icon) and the name stayed an untranslated key. Measured after the change: the item tag carries the texture name (`blockItem.tagHasIcon`). The test also names its block item exactly like its block, as vanilla does (`minecraft:stone` is both), which the engine's block -> item resolution for pick block relies on, and its resource pack now ships `texts/en_US.lang` / `texts/zh_CN.lang`

### Fixed

- The vanilla `minecraft:stick` losing its icon on a client (`Missing icon for data-driven item 'minecraft:stick'`) was caused by this repository's own `[REGISTRY]` suite: it exercised the vanilla item setters (`setIcon`, `setDisplayName`, `addTag`, `setFireResistant`, `setRepairItem`, `getAndModifyVanillaNetworkTagInfo`) on `minecraft:stick`, which fills `mModifiedVanillaItems` and makes `VanillaItemDefinitionSendHook` replace that item's `components` in the registry packet. The suite now modifies the item it registers itself (`modapi_test:test_item`) and only reads the vanilla one. Measured: with both mods loaded the stick's definition is no longer touched, and all four suites stay green. It was *not* the block item, the runtime packs, `ItemVersion::DataDriven`, item id collisions or the creative pass - each of those was ruled out by an A/B, several of which were invalid until the manifest explained why: `ModAPI` is `"passive": true` and `test` is its only dependent, so renaming `test` away also unloaded ModAPI (LeviLamina reported "loaded 0 mods"), which is what made "disable the test mod" look like a fix
- Follow-up to that, and the reason it was so damaging: `VanillaItemDefinitionSendHook` used to *replace* an item's whole `components` node with the tag recorded by the setters, and that tag is built from `Item::buildNetworkTag()` - for a data driven vanilla item nearly empty (measured: `minecraft:stick` came out as 37 chars holding only `minecraft:hand_equipped`). So modifying such an item dropped everything it had. The hook now *merges* the recorded changes into what the engine already publishes for the item (top level, and inside `item_properties`), which keeps the data the mod did not touch - the icon of a modified item, for instance

- Re-registering an item name left the replaced item in `ItemRegistry::mItemRegistry`: the new item was appended while the old one stayed in the vector, so every pass that walks it (the client definition build, and this API's own `forEachItemInRegistry`) saw two items answering to one name. The leftover carried none of the values its mod had applied (`max stack size` 64 instead of 16), and the object handed back by the registration differed from the one the registry exposed. Registration now drops the replaced entry, and a custom item's `_init` values survive - verified server side: `plain item fields: stack = 16, glint = true`, and the registry object and the registration product are the same pointer

- `/give <custom item>` silently produced nothing: the enum entry was written as `CommandItem{id, true, false}`, but the type is a union of `{ short mVersion; bool mOverrideAux; int mId; }` over a `uint64`. The id therefore landed in `mVersion` (truncated) and `mId` was left 0. Reading the engine's own `CommandItem::createInstance` settled the rest: it only treats `mId` as the item's own id when `mVersion` is **non-zero** - with 0 it converts the value as a *legacy* id through a different table, which is why an entry for `minecraft:stick` resolved to `minecraft:oak_sign` while our item resolved to nothing. The entry is now `{1, true, id}`. The registry suite builds that value and asserts `createInstance` returns the item (`give.*`)
- Note for whoever tests this next: the item must be reached by a **real** player - a virtual one gives `No targets matched selector`; and an item whose identifier contains a dot (`modapi_test:item.test_block`) is rejected by the command parser outright, so a block item registered under that name cannot be given at all

- The definition packet a client receives was never filled in for custom items. `ModAPI` hooked
  `ItemRegistryPacket::write`, but the old build shows the packet is serialized through
  `writeWithSerializationMode` reaching `serialize<ItemRegistryPacketPayload>::write` directly - `write` is a
  different virtual, so the hook never ran. It is now on the payload constructor, which is where `mItems` is
  produced and which every sending path goes through. Reading `components` out of an entry that has none also
  threw `bad variant access` and aborted the whole merge, so the node is only read after a check
- `ICustomItem` stopped setting `mItemParseVersion = ItemVersion::DataDriven`: it was commented out during an
  experiment and left that way. That flag is what gives an item the definition machinery a client needs, and
  custom items came up blank from then on (`Item <name> requires either an icon atlas or icon texture`)
- Re-registering an item name left the replaced item in `ItemRegistry::mItemRegistry`, and
  `CreativeItemRegistry` appended a creative entry per registration, so items showed up twice in a client's
  inventory and the leftover reported default fields. Registration now drops the replaced entry, and
  `registerCreativeItem` replaces the entry for an item instead of appending another
- `/give <custom item>` silently did nothing: the item enum entry was written as `CommandItem{id, true, false}`,
  but the type is a union of `{ short mVersion; bool mOverrideAux; int mId; }`. `CommandItem::createInstance`
  only reads `mId` as the item's own id when `mVersion` is non-zero - with 0 it converts the value as a *legacy*
  id through a different table
- A block item's id has to be the block's id folded into a short (`id <= 0xFF ? id : 255 - id`), which is what
  `ItemRegistryRef::registerBlockItem<BlockItem>` hands to `BlockItem`'s constructor. Assigning an id afterwards
  left the item's own state disagreeing with `mId`, which showed up as a creative entry nothing could be done
  with
- A block whose type a mod registers in C++ is not listed by the engine at all: the engine creates items for the
  blocks it parses out of documents, so that item is queued for the creative pass instead. The queue cannot
  duplicate anything now that `registerCreativeItem` replaces

- A block item drawn as the block itself (rather than as a flat picture) needs the `minecraft:block_placer`
  component in its definition, with the block it stands for. Two details decide whether a client acts on it, and
  both come out of `PlanterItemComponent::buildNetworkTag` in the old build, which writes `canUseBlockAsIcon` from
  the component's byte +64 and `replaceBlockItem` from +65: the keys are **camelCase** in the network form (a
  *document* uses `replace_block_item`, bound by `bindType`), and `canUseBlockAsIcon` has no document key at all -
  the definition sent to a client is the only place it can be set. A `minecraft:icon` must be left out at the same
  time: `ComponentItem::getIconInfo` reads the icon component first and only falls back to the item's block when
  there is none, so writing an icon pins the item to a texture

- A block registered from C++ now reaches clients without any document. The engine builds the list a client is
  told about (`StartGamePacketPayload::mBlockProperties`) out of the block definition group, and a block whose type
  ModAPI registered itself is not in that group - so the packet's write path adds the entry it is missing. The entry
  is field for field the one the engine writes itself (read out of a live one as SNBT), which is why it works: the
  same `components`/`minecraft:material_instances`, `menu_category`, `molangVersion` and `vanilla_block_data`, with
  the block's own id. `ServerBlockProperty`'s default constructor is declared in the header but not exported, so its
  definition is written here.
- The document path is gone with it: `registerBlockFromMemoryJson`, `registerBlockFromJsonFile`, `deferBlockDocument`
  and the runtime pack they installed were removed, along with the suppression of the engine's block type and the
  bookkeeping that went with it. A block is a `::BlockType` subclass registered under an identifier, and the texture
  a client draws it with is named with `BlockRegistry::setBlockTexture` - the texture file and the resource pack
  mappings (`blocks.json`, `terrain_texture.json`) are the mod's own, exactly as they are for an addon block.
- `ICustomBlock` is gone as well: it only forwarded a constructor to `::BlockType`, so a mod derives from
  `::BlockType` directly. `ICustomFeature` moved to `include/modapi/worldgen/base/` and
  `src/modapi/worldgen/base/`, which is where every other registry keeps its interfaces.

- The definition-level part of a block entry comes out of the registration's own `BlockDescription` instead of
  defaults: `menu_category.category` through the engine's exported
  `SharedTypes::v1_21_110::ItemCategory::stringFromCreativeItemCategory`, `group` and `is_hidden_in_commands` from
  `BlockMenuCategory`, and `vanilla_block_data.block_id` / `material` from `VanillaBlockData` (the material name
  through `BlockEnum::MaterialTypeToString`). `BlockPermutationDescription`s are published as `permutations`, each
  with its `condition` (`ExpressionNode::getExpressionString`), its components and its tags - the same three nodes the
  engine writes.
- A block is registered with what to tell a client about it, in one call and in the engine's own types:
  `registerBlock<MyBlock>("mymod:my_block", { .mArguments = {...}, .mProperty = {...} })`, with `BlockProperty` made
  of `BlockComponentGroupDescription` / `BlockDescription` / `BlockPermutationDescription`. The same property drives
  both sides: its components are injected into the running block type through `initializeComponentFromCode` - the
  route vanilla uses for its own blocks - and serialised for a client through `isNetworkComponent` + `getName` +
  `buildNetworkTag`, which are the calls `generateServerBlockProperties` makes. The entry produced this way matches
  the engine's own field for field (measured against the entry the engine writes for an addon block: identical apart
  from each block's own texture name and id). No `setBlockTexture`/`setBlockProperty`: a property is handed over with
  the registration.
- The engine declares default constructors and copy operations for those definition types and does not export them,
  so `include/modapi/block/base/EngineDefaults.h` supplies the definitions inline - default constructors only, and
  copy/assignment only where the linker proved them missing. A type holding a `CompoundTag`
  (`ServerBlockProperty`) keeps the engine's exported copy: a memberwise `= default` there copies the tag shallowly
  and two objects then free the same storage, which crashed the server at `StartGamePacket` time.

### Changed

- Adapted LeviLamina 26.40.0 and BDS 26.40.8
- Replaced `CraftingDataPacket::prepareFromRecipes` with `CraftingDataPacketPayload::fromRecipes`, which was renamed in 26.40
- Rebuilt the smithing recipes by hand: 26.40 only declares the `SmithingTransformRecipe` / `SmithingTrimRecipe` constructors for the client, so the recipe fields are filled in directly
- Followed the `ResourcePackStack::deserialize` signature change (stream to `std::string_view`) and the `PackManifest` header move
- Dropped the now redundant local definitions of `ItemInstance::ItemInstance` and `CreativeGroupInfo::~CreativeGroupInfo`

## [26.20.0] - 2026-07-09

- The test ships a real 16x16 PNG for its block texture (embedded, generated with Pillow, also kept at `src-test/assets/modapi_test_block.png`) instead of a placeholder file, and asserts the shipped file carries the PNG signature: a placeholder leaves a client with nothing to draw, which was a real defect spotted in review

### Added

- `modapi::item::ICustomBlockItem` (`include/modapi/item/base/ICustomBlockItem.h`): the item that places a custom block. It derives from the engine's `::BlockItem` and is registered through `ItemRegistry` like any other item (`isEntry` already accepted `std::derived_from<Entry, ::Item>`, so nothing had to be loosened), with an `initCustomItem(ICustomBlockItem&)` overload next to the existing `ICustomItem`/`ICustomArmorItem` ones. Measured: `::BlockItem` does *not* link itself to its block (`getBlockTypeForRendering` is not even overridden by it) - the link is `::Item::mBlockType`, so the class sets it in `_init`; before that an item built for a custom block reported `minecraft:air`, afterwards it reports the block. Registration timing needed no change either: block types are registered right after the level reads its definitions, which is measured to be before vanilla builds its items, so a block item finds its block; an earlier attempt to move block registration into the item pass was reverted, along with the switch to a ModAPI owned id range
- `src-test/BlockTest.cpp` overrides virtuals on both sides and asserts the effect: the block item's engine fields (max stack size 16, foil), an engine virtual (`isMusicDisk`), the item -> block link (`mBlockType`), and a block hook (`canProvideSupport`). A plain `ICustomItem`'s overrides are only *recorded*, with the reason: its engine object is replaced by the engine in a later pass (measured: the registry holds a different pointer than the one ModAPI built), so its fields read the defaults - a pre-existing ModAPI issue, out of scope here. Note also that this build has no RTTI data, so `dynamic_cast` aborts ("no RTTI data") and a `static_cast` on a pointer you own is the way to go

- Block items now send a client definition of their own: `NetworkTagBuilder` gained a `buildClientComponents(ICustomBlockItem const&)` overload and `ICustomBlockItem` gained `getIcon`, `getDisplayName` and a `buildNetworkTag` override. Without it the creative entry for a block item had no texture (the block itself rendered, so the packs were delivered - the item definition simply had no icon) and the name stayed an untranslated key. Measured after the change: the item tag carries the texture name (`blockItem.tagHasIcon`). The test also names its block item exactly like its block, as vanilla does (`minecraft:stone` is both), which the engine's block -> item resolution for pick block relies on, and its resource pack now ships `texts/en_US.lang` / `texts/zh_CN.lang`

### Fixed

- The vanilla `minecraft:stick` losing its icon on a client (`Missing icon for data-driven item 'minecraft:stick'`) was caused by this repository's own `[REGISTRY]` suite: it exercised the vanilla item setters (`setIcon`, `setDisplayName`, `addTag`, `setFireResistant`, `setRepairItem`, `getAndModifyVanillaNetworkTagInfo`) on `minecraft:stick`, which fills `mModifiedVanillaItems` and makes `VanillaItemDefinitionSendHook` replace that item's `components` in the registry packet. The suite now modifies the item it registers itself (`modapi_test:test_item`) and only reads the vanilla one. Measured: with both mods loaded the stick's definition is no longer touched, and all four suites stay green. It was *not* the block item, the runtime packs, `ItemVersion::DataDriven`, item id collisions or the creative pass - each of those was ruled out by an A/B, several of which were invalid until the manifest explained why: `ModAPI` is `"passive": true` and `test` is its only dependent, so renaming `test` away also unloaded ModAPI (LeviLamina reported "loaded 0 mods"), which is what made "disable the test mod" look like a fix
- Follow-up to that, and the reason it was so damaging: `VanillaItemDefinitionSendHook` used to *replace* an item's whole `components` node with the tag recorded by the setters, and that tag is built from `Item::buildNetworkTag()` - for a data driven vanilla item nearly empty (measured: `minecraft:stick` came out as 37 chars holding only `minecraft:hand_equipped`). So modifying such an item dropped everything it had. The hook now *merges* the recorded changes into what the engine already publishes for the item (top level, and inside `item_properties`), which keeps the data the mod did not touch - the icon of a modified item, for instance

- Re-registering an item name left the replaced item in `ItemRegistry::mItemRegistry`: the new item was appended while the old one stayed in the vector, so every pass that walks it (the client definition build, and this API's own `forEachItemInRegistry`) saw two items answering to one name. The leftover carried none of the values its mod had applied (`max stack size` 64 instead of 16), and the object handed back by the registration differed from the one the registry exposed. Registration now drops the replaced entry, and a custom item's `_init` values survive - verified server side: `plain item fields: stack = 16, glint = true`, and the registry object and the registration product are the same pointer

- `/give <custom item>` silently produced nothing: the enum entry was written as `CommandItem{id, true, false}`, but the type is a union of `{ short mVersion; bool mOverrideAux; int mId; }` over a `uint64`. The id therefore landed in `mVersion` (truncated) and `mId` was left 0. Reading the engine's own `CommandItem::createInstance` settled the rest: it only treats `mId` as the item's own id when `mVersion` is **non-zero** - with 0 it converts the value as a *legacy* id through a different table, which is why an entry for `minecraft:stick` resolved to `minecraft:oak_sign` while our item resolved to nothing. The entry is now `{1, true, id}`. The registry suite builds that value and asserts `createInstance` returns the item (`give.*`)
- Note for whoever tests this next: the item must be reached by a **real** player - a virtual one gives `No targets matched selector`; and an item whose identifier contains a dot (`modapi_test:item.test_block`) is rejected by the command parser outright, so a block item registered under that name cannot be given at all

- The definition packet a client receives was never filled in for custom items. `ModAPI` hooked
  `ItemRegistryPacket::write`, but the old build shows the packet is serialized through
  `writeWithSerializationMode` reaching `serialize<ItemRegistryPacketPayload>::write` directly - `write` is a
  different virtual, so the hook never ran. It is now on the payload constructor, which is where `mItems` is
  produced and which every sending path goes through. Reading `components` out of an entry that has none also
  threw `bad variant access` and aborted the whole merge, so the node is only read after a check
- `ICustomItem` stopped setting `mItemParseVersion = ItemVersion::DataDriven`: it was commented out during an
  experiment and left that way. That flag is what gives an item the definition machinery a client needs, and
  custom items came up blank from then on (`Item <name> requires either an icon atlas or icon texture`)
- Re-registering an item name left the replaced item in `ItemRegistry::mItemRegistry`, and
  `CreativeItemRegistry` appended a creative entry per registration, so items showed up twice in a client's
  inventory and the leftover reported default fields. Registration now drops the replaced entry, and
  `registerCreativeItem` replaces the entry for an item instead of appending another
- `/give <custom item>` silently did nothing: the item enum entry was written as `CommandItem{id, true, false}`,
  but the type is a union of `{ short mVersion; bool mOverrideAux; int mId; }`. `CommandItem::createInstance`
  only reads `mId` as the item's own id when `mVersion` is non-zero - with 0 it converts the value as a *legacy*
  id through a different table
- A block item's id has to be the block's id folded into a short (`id <= 0xFF ? id : 255 - id`), which is what
  `ItemRegistryRef::registerBlockItem<BlockItem>` hands to `BlockItem`'s constructor. Assigning an id afterwards
  left the item's own state disagreeing with `mId`, which showed up as a creative entry nothing could be done
  with
- A block whose type a mod registers in C++ is not listed by the engine at all: the engine creates items for the
  blocks it parses out of documents, so that item is queued for the creative pass instead. The queue cannot
  duplicate anything now that `registerCreativeItem` replaces

- A block item drawn as the block itself (rather than as a flat picture) needs the `minecraft:block_placer`
  component in its definition, with the block it stands for. Two details decide whether a client acts on it, and
  both come out of `PlanterItemComponent::buildNetworkTag` in the old build, which writes `canUseBlockAsIcon` from
  the component's byte +64 and `replaceBlockItem` from +65: the keys are **camelCase** in the network form (a
  *document* uses `replace_block_item`, bound by `bindType`), and `canUseBlockAsIcon` has no document key at all -
  the definition sent to a client is the only place it can be set. A `minecraft:icon` must be left out at the same
  time: `ComponentItem::getIconInfo` reads the icon component first and only falls back to the item's block when
  there is none, so writing an icon pins the item to a texture

- A block registered from C++ now reaches clients without any document. The engine builds the list a client is
  told about (`StartGamePacketPayload::mBlockProperties`) out of the block definition group, and a block whose type
  ModAPI registered itself is not in that group - so the packet's write path adds the entry it is missing. The entry
  is field for field the one the engine writes itself (read out of a live one as SNBT), which is why it works: the
  same `components`/`minecraft:material_instances`, `menu_category`, `molangVersion` and `vanilla_block_data`, with
  the block's own id. `ServerBlockProperty`'s default constructor is declared in the header but not exported, so its
  definition is written here.
- The document path is gone with it: `registerBlockFromMemoryJson`, `registerBlockFromJsonFile`, `deferBlockDocument`
  and the runtime pack they installed were removed, along with the suppression of the engine's block type and the
  bookkeeping that went with it. A block is a `::BlockType` subclass registered under an identifier, and the texture
  a client draws it with is named with `BlockRegistry::setBlockTexture` - the texture file and the resource pack
  mappings (`blocks.json`, `terrain_texture.json`) are the mod's own, exactly as they are for an addon block.
- `ICustomBlock` is gone as well: it only forwarded a constructor to `::BlockType`, so a mod derives from
  `::BlockType` directly. `ICustomFeature` moved to `include/modapi/worldgen/base/` and
  `src/modapi/worldgen/base/`, which is where every other registry keeps its interfaces.

- The definition-level part of a block entry comes out of the registration's own `BlockDescription` instead of
  defaults: `menu_category.category` through the engine's exported
  `SharedTypes::v1_21_110::ItemCategory::stringFromCreativeItemCategory`, `group` and `is_hidden_in_commands` from
  `BlockMenuCategory`, and `vanilla_block_data.block_id` / `material` from `VanillaBlockData` (the material name
  through `BlockEnum::MaterialTypeToString`). `BlockPermutationDescription`s are published as `permutations`, each
  with its `condition` (`ExpressionNode::getExpressionString`), its components and its tags - the same three nodes the
  engine writes.
- A block is registered with what to tell a client about it, in one call and in the engine's own types:
  `registerBlock<MyBlock>("mymod:my_block", { .mArguments = {...}, .mProperty = {...} })`, with `BlockProperty` made
  of `BlockComponentGroupDescription` / `BlockDescription` / `BlockPermutationDescription`. The same property drives
  both sides: its components are injected into the running block type through `initializeComponentFromCode` - the
  route vanilla uses for its own blocks - and serialised for a client through `isNetworkComponent` + `getName` +
  `buildNetworkTag`, which are the calls `generateServerBlockProperties` makes. The entry produced this way matches
  the engine's own field for field (measured against the entry the engine writes for an addon block: identical apart
  from each block's own texture name and id). No `setBlockTexture`/`setBlockProperty`: a property is handed over with
  the registration.
- The engine declares default constructors and copy operations for those definition types and does not export them,
  so `include/modapi/block/base/EngineDefaults.h` supplies the definitions inline - default constructors only, and
  copy/assignment only where the linker proved them missing. A type holding a `CompoundTag`
  (`ServerBlockProperty`) keeps the engine's exported copy: a memberwise `= default` there copies the tag shallowly
  and two objects then free the same storage, which crashed the server at `StartGamePacket` time.

### Changed

- `RuntimePack` gets a fresh uuid on every construction instead of a name derived one (and logs it): a client that already has a pack with the same uuid does not download it again, so a pack rebuilt on each server start has to offer an identity no client has seen. Packs also have to be installed while mods load - the stack clients are offered is composed when a level starts, so a pack installed later (from a command, for instance) never reaches them; the test installs its packs there now

- The test ships a real 16x16 PNG for its block texture (embedded, generated with Pillow, also kept at `src-test/assets/modapi_test_block.png`) instead of a placeholder file, and asserts the shipped file carries the PNG signature: a placeholder leaves a client with nothing to draw, which was a real defect spotted in review

### Added

- `modapi::item::ICustomBlockItem` (`include/modapi/item/base/ICustomBlockItem.h`): the item that places a custom block. It derives from the engine's `::BlockItem` and is registered through `ItemRegistry` like any other item (`isEntry` already accepted `std::derived_from<Entry, ::Item>`, so nothing had to be loosened), with an `initCustomItem(ICustomBlockItem&)` overload next to the existing `ICustomItem`/`ICustomArmorItem` ones. Measured: `::BlockItem` does *not* link itself to its block (`getBlockTypeForRendering` is not even overridden by it) - the link is `::Item::mBlockType`, so the class sets it in `_init`; before that an item built for a custom block reported `minecraft:air`, afterwards it reports the block. Registration timing needed no change either: block types are registered right after the level reads its definitions, which is measured to be before vanilla builds its items, so a block item finds its block; an earlier attempt to move block registration into the item pass was reverted, along with the switch to a ModAPI owned id range
- `src-test/BlockTest.cpp` overrides virtuals on both sides and asserts the effect: the block item's engine fields (max stack size 16, foil), an engine virtual (`isMusicDisk`), the item -> block link (`mBlockType`), and a block hook (`canProvideSupport`). A plain `ICustomItem`'s overrides are only *recorded*, with the reason: its engine object is replaced by the engine in a later pass (measured: the registry holds a different pointer than the one ModAPI built), so its fields read the defaults - a pre-existing ModAPI issue, out of scope here. Note also that this build has no RTTI data, so `dynamic_cast` aborts ("no RTTI data") and a `static_cast` on a pointer you own is the way to go

- Block items now send a client definition of their own: `NetworkTagBuilder` gained a `buildClientComponents(ICustomBlockItem const&)` overload and `ICustomBlockItem` gained `getIcon`, `getDisplayName` and a `buildNetworkTag` override. Without it the creative entry for a block item had no texture (the block itself rendered, so the packs were delivered - the item definition simply had no icon) and the name stayed an untranslated key. Measured after the change: the item tag carries the texture name (`blockItem.tagHasIcon`). The test also names its block item exactly like its block, as vanilla does (`minecraft:stone` is both), which the engine's block -> item resolution for pick block relies on, and its resource pack now ships `texts/en_US.lang` / `texts/zh_CN.lang`

### Fixed

- The vanilla `minecraft:stick` losing its icon on a client (`Missing icon for data-driven item 'minecraft:stick'`) was caused by this repository's own `[REGISTRY]` suite: it exercised the vanilla item setters (`setIcon`, `setDisplayName`, `addTag`, `setFireResistant`, `setRepairItem`, `getAndModifyVanillaNetworkTagInfo`) on `minecraft:stick`, which fills `mModifiedVanillaItems` and makes `VanillaItemDefinitionSendHook` replace that item's `components` in the registry packet. The suite now modifies the item it registers itself (`modapi_test:test_item`) and only reads the vanilla one. Measured: with both mods loaded the stick's definition is no longer touched, and all four suites stay green. It was *not* the block item, the runtime packs, `ItemVersion::DataDriven`, item id collisions or the creative pass - each of those was ruled out by an A/B, several of which were invalid until the manifest explained why: `ModAPI` is `"passive": true` and `test` is its only dependent, so renaming `test` away also unloaded ModAPI (LeviLamina reported "loaded 0 mods"), which is what made "disable the test mod" look like a fix
- Follow-up to that, and the reason it was so damaging: `VanillaItemDefinitionSendHook` used to *replace* an item's whole `components` node with the tag recorded by the setters, and that tag is built from `Item::buildNetworkTag()` - for a data driven vanilla item nearly empty (measured: `minecraft:stick` came out as 37 chars holding only `minecraft:hand_equipped`). So modifying such an item dropped everything it had. The hook now *merges* the recorded changes into what the engine already publishes for the item (top level, and inside `item_properties`), which keeps the data the mod did not touch - the icon of a modified item, for instance

- Re-registering an item name left the replaced item in `ItemRegistry::mItemRegistry`: the new item was appended while the old one stayed in the vector, so every pass that walks it (the client definition build, and this API's own `forEachItemInRegistry`) saw two items answering to one name. The leftover carried none of the values its mod had applied (`max stack size` 64 instead of 16), and the object handed back by the registration differed from the one the registry exposed. Registration now drops the replaced entry, and a custom item's `_init` values survive - verified server side: `plain item fields: stack = 16, glint = true`, and the registry object and the registration product are the same pointer

- `/give <custom item>` silently produced nothing: the enum entry was written as `CommandItem{id, true, false}`, but the type is a union of `{ short mVersion; bool mOverrideAux; int mId; }` over a `uint64`. The id therefore landed in `mVersion` (truncated) and `mId` was left 0. Reading the engine's own `CommandItem::createInstance` settled the rest: it only treats `mId` as the item's own id when `mVersion` is **non-zero** - with 0 it converts the value as a *legacy* id through a different table, which is why an entry for `minecraft:stick` resolved to `minecraft:oak_sign` while our item resolved to nothing. The entry is now `{1, true, id}`. The registry suite builds that value and asserts `createInstance` returns the item (`give.*`)
- Note for whoever tests this next: the item must be reached by a **real** player - a virtual one gives `No targets matched selector`; and an item whose identifier contains a dot (`modapi_test:item.test_block`) is rejected by the command parser outright, so a block item registered under that name cannot be given at all

- The definition packet a client receives was never filled in for custom items. `ModAPI` hooked
  `ItemRegistryPacket::write`, but the old build shows the packet is serialized through
  `writeWithSerializationMode` reaching `serialize<ItemRegistryPacketPayload>::write` directly - `write` is a
  different virtual, so the hook never ran. It is now on the payload constructor, which is where `mItems` is
  produced and which every sending path goes through. Reading `components` out of an entry that has none also
  threw `bad variant access` and aborted the whole merge, so the node is only read after a check
- `ICustomItem` stopped setting `mItemParseVersion = ItemVersion::DataDriven`: it was commented out during an
  experiment and left that way. That flag is what gives an item the definition machinery a client needs, and
  custom items came up blank from then on (`Item <name> requires either an icon atlas or icon texture`)
- Re-registering an item name left the replaced item in `ItemRegistry::mItemRegistry`, and
  `CreativeItemRegistry` appended a creative entry per registration, so items showed up twice in a client's
  inventory and the leftover reported default fields. Registration now drops the replaced entry, and
  `registerCreativeItem` replaces the entry for an item instead of appending another
- `/give <custom item>` silently did nothing: the item enum entry was written as `CommandItem{id, true, false}`,
  but the type is a union of `{ short mVersion; bool mOverrideAux; int mId; }`. `CommandItem::createInstance`
  only reads `mId` as the item's own id when `mVersion` is non-zero - with 0 it converts the value as a *legacy*
  id through a different table
- A block item's id has to be the block's id folded into a short (`id <= 0xFF ? id : 255 - id`), which is what
  `ItemRegistryRef::registerBlockItem<BlockItem>` hands to `BlockItem`'s constructor. Assigning an id afterwards
  left the item's own state disagreeing with `mId`, which showed up as a creative entry nothing could be done
  with
- A block whose type a mod registers in C++ is not listed by the engine at all: the engine creates items for the
  blocks it parses out of documents, so that item is queued for the creative pass instead. The queue cannot
  duplicate anything now that `registerCreativeItem` replaces

- A block item drawn as the block itself (rather than as a flat picture) needs the `minecraft:block_placer`
  component in its definition, with the block it stands for. Two details decide whether a client acts on it, and
  both come out of `PlanterItemComponent::buildNetworkTag` in the old build, which writes `canUseBlockAsIcon` from
  the component's byte +64 and `replaceBlockItem` from +65: the keys are **camelCase** in the network form (a
  *document* uses `replace_block_item`, bound by `bindType`), and `canUseBlockAsIcon` has no document key at all -
  the definition sent to a client is the only place it can be set. A `minecraft:icon` must be left out at the same
  time: `ComponentItem::getIconInfo` reads the icon component first and only falls back to the item's block when
  there is none, so writing an icon pins the item to a texture

- A block registered from C++ now reaches clients without any document. The engine builds the list a client is
  told about (`StartGamePacketPayload::mBlockProperties`) out of the block definition group, and a block whose type
  ModAPI registered itself is not in that group - so the packet's write path adds the entry it is missing. The entry
  is field for field the one the engine writes itself (read out of a live one as SNBT), which is why it works: the
  same `components`/`minecraft:material_instances`, `menu_category`, `molangVersion` and `vanilla_block_data`, with
  the block's own id. `ServerBlockProperty`'s default constructor is declared in the header but not exported, so its
  definition is written here.
- The document path is gone with it: `registerBlockFromMemoryJson`, `registerBlockFromJsonFile`, `deferBlockDocument`
  and the runtime pack they installed were removed, along with the suppression of the engine's block type and the
  bookkeeping that went with it. A block is a `::BlockType` subclass registered under an identifier, and the texture
  a client draws it with is named with `BlockRegistry::setBlockTexture` - the texture file and the resource pack
  mappings (`blocks.json`, `terrain_texture.json`) are the mod's own, exactly as they are for an addon block.
- `ICustomBlock` is gone as well: it only forwarded a constructor to `::BlockType`, so a mod derives from
  `::BlockType` directly. `ICustomFeature` moved to `include/modapi/worldgen/base/` and
  `src/modapi/worldgen/base/`, which is where every other registry keeps its interfaces.

- The definition-level part of a block entry comes out of the registration's own `BlockDescription` instead of
  defaults: `menu_category.category` through the engine's exported
  `SharedTypes::v1_21_110::ItemCategory::stringFromCreativeItemCategory`, `group` and `is_hidden_in_commands` from
  `BlockMenuCategory`, and `vanilla_block_data.block_id` / `material` from `VanillaBlockData` (the material name
  through `BlockEnum::MaterialTypeToString`). `BlockPermutationDescription`s are published as `permutations`, each
  with its `condition` (`ExpressionNode::getExpressionString`), its components and its tags - the same three nodes the
  engine writes.
- A block is registered with what to tell a client about it, in one call and in the engine's own types:
  `registerBlock<MyBlock>("mymod:my_block", { .mArguments = {...}, .mProperty = {...} })`, with `BlockProperty` made
  of `BlockComponentGroupDescription` / `BlockDescription` / `BlockPermutationDescription`. The same property drives
  both sides: its components are injected into the running block type through `initializeComponentFromCode` - the
  route vanilla uses for its own blocks - and serialised for a client through `isNetworkComponent` + `getName` +
  `buildNetworkTag`, which are the calls `generateServerBlockProperties` makes. The entry produced this way matches
  the engine's own field for field (measured against the entry the engine writes for an addon block: identical apart
  from each block's own texture name and id). No `setBlockTexture`/`setBlockProperty`: a property is handed over with
  the registration.
- The engine declares default constructors and copy operations for those definition types and does not export them,
  so `include/modapi/block/base/EngineDefaults.h` supplies the definitions inline - default constructors only, and
  copy/assignment only where the linker proved them missing. A type holding a `CompoundTag`
  (`ServerBlockProperty`) keeps the engine's exported copy: a memberwise `= default` there copies the tag shallowly
  and two objects then free the same storage, which crashed the server at `StartGamePacket` time.

### Changed

- Adapted LeviLamina 26.20.0
- Relicensed the project under AGPL-3.0-or-later and added a README

## [26.10.0] - 2026-04-10

- The test ships a real 16x16 PNG for its block texture (embedded, generated with Pillow, also kept at `src-test/assets/modapi_test_block.png`) instead of a placeholder file, and asserts the shipped file carries the PNG signature: a placeholder leaves a client with nothing to draw, which was a real defect spotted in review

### Added

- `modapi::item::ICustomBlockItem` (`include/modapi/item/base/ICustomBlockItem.h`): the item that places a custom block. It derives from the engine's `::BlockItem` and is registered through `ItemRegistry` like any other item (`isEntry` already accepted `std::derived_from<Entry, ::Item>`, so nothing had to be loosened), with an `initCustomItem(ICustomBlockItem&)` overload next to the existing `ICustomItem`/`ICustomArmorItem` ones. Measured: `::BlockItem` does *not* link itself to its block (`getBlockTypeForRendering` is not even overridden by it) - the link is `::Item::mBlockType`, so the class sets it in `_init`; before that an item built for a custom block reported `minecraft:air`, afterwards it reports the block. Registration timing needed no change either: block types are registered right after the level reads its definitions, which is measured to be before vanilla builds its items, so a block item finds its block; an earlier attempt to move block registration into the item pass was reverted, along with the switch to a ModAPI owned id range
- `src-test/BlockTest.cpp` overrides virtuals on both sides and asserts the effect: the block item's engine fields (max stack size 16, foil), an engine virtual (`isMusicDisk`), the item -> block link (`mBlockType`), and a block hook (`canProvideSupport`). A plain `ICustomItem`'s overrides are only *recorded*, with the reason: its engine object is replaced by the engine in a later pass (measured: the registry holds a different pointer than the one ModAPI built), so its fields read the defaults - a pre-existing ModAPI issue, out of scope here. Note also that this build has no RTTI data, so `dynamic_cast` aborts ("no RTTI data") and a `static_cast` on a pointer you own is the way to go

- Block items now send a client definition of their own: `NetworkTagBuilder` gained a `buildClientComponents(ICustomBlockItem const&)` overload and `ICustomBlockItem` gained `getIcon`, `getDisplayName` and a `buildNetworkTag` override. Without it the creative entry for a block item had no texture (the block itself rendered, so the packs were delivered - the item definition simply had no icon) and the name stayed an untranslated key. Measured after the change: the item tag carries the texture name (`blockItem.tagHasIcon`). The test also names its block item exactly like its block, as vanilla does (`minecraft:stone` is both), which the engine's block -> item resolution for pick block relies on, and its resource pack now ships `texts/en_US.lang` / `texts/zh_CN.lang`

### Fixed

- The vanilla `minecraft:stick` losing its icon on a client (`Missing icon for data-driven item 'minecraft:stick'`) was caused by this repository's own `[REGISTRY]` suite: it exercised the vanilla item setters (`setIcon`, `setDisplayName`, `addTag`, `setFireResistant`, `setRepairItem`, `getAndModifyVanillaNetworkTagInfo`) on `minecraft:stick`, which fills `mModifiedVanillaItems` and makes `VanillaItemDefinitionSendHook` replace that item's `components` in the registry packet. The suite now modifies the item it registers itself (`modapi_test:test_item`) and only reads the vanilla one. Measured: with both mods loaded the stick's definition is no longer touched, and all four suites stay green. It was *not* the block item, the runtime packs, `ItemVersion::DataDriven`, item id collisions or the creative pass - each of those was ruled out by an A/B, several of which were invalid until the manifest explained why: `ModAPI` is `"passive": true` and `test` is its only dependent, so renaming `test` away also unloaded ModAPI (LeviLamina reported "loaded 0 mods"), which is what made "disable the test mod" look like a fix
- Follow-up to that, and the reason it was so damaging: `VanillaItemDefinitionSendHook` used to *replace* an item's whole `components` node with the tag recorded by the setters, and that tag is built from `Item::buildNetworkTag()` - for a data driven vanilla item nearly empty (measured: `minecraft:stick` came out as 37 chars holding only `minecraft:hand_equipped`). So modifying such an item dropped everything it had. The hook now *merges* the recorded changes into what the engine already publishes for the item (top level, and inside `item_properties`), which keeps the data the mod did not touch - the icon of a modified item, for instance

- Re-registering an item name left the replaced item in `ItemRegistry::mItemRegistry`: the new item was appended while the old one stayed in the vector, so every pass that walks it (the client definition build, and this API's own `forEachItemInRegistry`) saw two items answering to one name. The leftover carried none of the values its mod had applied (`max stack size` 64 instead of 16), and the object handed back by the registration differed from the one the registry exposed. Registration now drops the replaced entry, and a custom item's `_init` values survive - verified server side: `plain item fields: stack = 16, glint = true`, and the registry object and the registration product are the same pointer

- `/give <custom item>` silently produced nothing: the enum entry was written as `CommandItem{id, true, false}`, but the type is a union of `{ short mVersion; bool mOverrideAux; int mId; }` over a `uint64`. The id therefore landed in `mVersion` (truncated) and `mId` was left 0. Reading the engine's own `CommandItem::createInstance` settled the rest: it only treats `mId` as the item's own id when `mVersion` is **non-zero** - with 0 it converts the value as a *legacy* id through a different table, which is why an entry for `minecraft:stick` resolved to `minecraft:oak_sign` while our item resolved to nothing. The entry is now `{1, true, id}`. The registry suite builds that value and asserts `createInstance` returns the item (`give.*`)
- Note for whoever tests this next: the item must be reached by a **real** player - a virtual one gives `No targets matched selector`; and an item whose identifier contains a dot (`modapi_test:item.test_block`) is rejected by the command parser outright, so a block item registered under that name cannot be given at all

- The definition packet a client receives was never filled in for custom items. `ModAPI` hooked
  `ItemRegistryPacket::write`, but the old build shows the packet is serialized through
  `writeWithSerializationMode` reaching `serialize<ItemRegistryPacketPayload>::write` directly - `write` is a
  different virtual, so the hook never ran. It is now on the payload constructor, which is where `mItems` is
  produced and which every sending path goes through. Reading `components` out of an entry that has none also
  threw `bad variant access` and aborted the whole merge, so the node is only read after a check
- `ICustomItem` stopped setting `mItemParseVersion = ItemVersion::DataDriven`: it was commented out during an
  experiment and left that way. That flag is what gives an item the definition machinery a client needs, and
  custom items came up blank from then on (`Item <name> requires either an icon atlas or icon texture`)
- Re-registering an item name left the replaced item in `ItemRegistry::mItemRegistry`, and
  `CreativeItemRegistry` appended a creative entry per registration, so items showed up twice in a client's
  inventory and the leftover reported default fields. Registration now drops the replaced entry, and
  `registerCreativeItem` replaces the entry for an item instead of appending another
- `/give <custom item>` silently did nothing: the item enum entry was written as `CommandItem{id, true, false}`,
  but the type is a union of `{ short mVersion; bool mOverrideAux; int mId; }`. `CommandItem::createInstance`
  only reads `mId` as the item's own id when `mVersion` is non-zero - with 0 it converts the value as a *legacy*
  id through a different table
- A block item's id has to be the block's id folded into a short (`id <= 0xFF ? id : 255 - id`), which is what
  `ItemRegistryRef::registerBlockItem<BlockItem>` hands to `BlockItem`'s constructor. Assigning an id afterwards
  left the item's own state disagreeing with `mId`, which showed up as a creative entry nothing could be done
  with
- A block whose type a mod registers in C++ is not listed by the engine at all: the engine creates items for the
  blocks it parses out of documents, so that item is queued for the creative pass instead. The queue cannot
  duplicate anything now that `registerCreativeItem` replaces

- A block item drawn as the block itself (rather than as a flat picture) needs the `minecraft:block_placer`
  component in its definition, with the block it stands for. Two details decide whether a client acts on it, and
  both come out of `PlanterItemComponent::buildNetworkTag` in the old build, which writes `canUseBlockAsIcon` from
  the component's byte +64 and `replaceBlockItem` from +65: the keys are **camelCase** in the network form (a
  *document* uses `replace_block_item`, bound by `bindType`), and `canUseBlockAsIcon` has no document key at all -
  the definition sent to a client is the only place it can be set. A `minecraft:icon` must be left out at the same
  time: `ComponentItem::getIconInfo` reads the icon component first and only falls back to the item's block when
  there is none, so writing an icon pins the item to a texture

- A block registered from C++ now reaches clients without any document. The engine builds the list a client is
  told about (`StartGamePacketPayload::mBlockProperties`) out of the block definition group, and a block whose type
  ModAPI registered itself is not in that group - so the packet's write path adds the entry it is missing. The entry
  is field for field the one the engine writes itself (read out of a live one as SNBT), which is why it works: the
  same `components`/`minecraft:material_instances`, `menu_category`, `molangVersion` and `vanilla_block_data`, with
  the block's own id. `ServerBlockProperty`'s default constructor is declared in the header but not exported, so its
  definition is written here.
- The document path is gone with it: `registerBlockFromMemoryJson`, `registerBlockFromJsonFile`, `deferBlockDocument`
  and the runtime pack they installed were removed, along with the suppression of the engine's block type and the
  bookkeeping that went with it. A block is a `::BlockType` subclass registered under an identifier, and the texture
  a client draws it with is named with `BlockRegistry::setBlockTexture` - the texture file and the resource pack
  mappings (`blocks.json`, `terrain_texture.json`) are the mod's own, exactly as they are for an addon block.
- `ICustomBlock` is gone as well: it only forwarded a constructor to `::BlockType`, so a mod derives from
  `::BlockType` directly. `ICustomFeature` moved to `include/modapi/worldgen/base/` and
  `src/modapi/worldgen/base/`, which is where every other registry keeps its interfaces.

- The definition-level part of a block entry comes out of the registration's own `BlockDescription` instead of
  defaults: `menu_category.category` through the engine's exported
  `SharedTypes::v1_21_110::ItemCategory::stringFromCreativeItemCategory`, `group` and `is_hidden_in_commands` from
  `BlockMenuCategory`, and `vanilla_block_data.block_id` / `material` from `VanillaBlockData` (the material name
  through `BlockEnum::MaterialTypeToString`). `BlockPermutationDescription`s are published as `permutations`, each
  with its `condition` (`ExpressionNode::getExpressionString`), its components and its tags - the same three nodes the
  engine writes.
- A block is registered with what to tell a client about it, in one call and in the engine's own types:
  `registerBlock<MyBlock>("mymod:my_block", { .mArguments = {...}, .mProperty = {...} })`, with `BlockProperty` made
  of `BlockComponentGroupDescription` / `BlockDescription` / `BlockPermutationDescription`. The same property drives
  both sides: its components are injected into the running block type through `initializeComponentFromCode` - the
  route vanilla uses for its own blocks - and serialised for a client through `isNetworkComponent` + `getName` +
  `buildNetworkTag`, which are the calls `generateServerBlockProperties` makes. The entry produced this way matches
  the engine's own field for field (measured against the entry the engine writes for an addon block: identical apart
  from each block's own texture name and id). No `setBlockTexture`/`setBlockProperty`: a property is handed over with
  the registration.
- The engine declares default constructors and copy operations for those definition types and does not export them,
  so `include/modapi/block/base/EngineDefaults.h` supplies the definitions inline - default constructors only, and
  copy/assignment only where the linker proved them missing. A type holding a `CompoundTag`
  (`ServerBlockProperty`) keeps the engine's exported copy: a memberwise `= default` there copies the tag shallowly
  and two objects then free the same storage, which crashed the server at `StartGamePacket` time.

### Changed

- `RuntimePack` gets a fresh uuid on every construction instead of a name derived one (and logs it): a client that already has a pack with the same uuid does not download it again, so a pack rebuilt on each server start has to offer an identity no client has seen. Packs also have to be installed while mods load - the stack clients are offered is composed when a level starts, so a pack installed later (from a command, for instance) never reaches them; the test installs its packs there now

- The test ships a real 16x16 PNG for its block texture (embedded, generated with Pillow, also kept at `src-test/assets/modapi_test_block.png`) instead of a placeholder file, and asserts the shipped file carries the PNG signature: a placeholder leaves a client with nothing to draw, which was a real defect spotted in review

### Added

- `modapi::item::ICustomBlockItem` (`include/modapi/item/base/ICustomBlockItem.h`): the item that places a custom block. It derives from the engine's `::BlockItem` and is registered through `ItemRegistry` like any other item (`isEntry` already accepted `std::derived_from<Entry, ::Item>`, so nothing had to be loosened), with an `initCustomItem(ICustomBlockItem&)` overload next to the existing `ICustomItem`/`ICustomArmorItem` ones. Measured: `::BlockItem` does *not* link itself to its block (`getBlockTypeForRendering` is not even overridden by it) - the link is `::Item::mBlockType`, so the class sets it in `_init`; before that an item built for a custom block reported `minecraft:air`, afterwards it reports the block. Registration timing needed no change either: block types are registered right after the level reads its definitions, which is measured to be before vanilla builds its items, so a block item finds its block; an earlier attempt to move block registration into the item pass was reverted, along with the switch to a ModAPI owned id range
- `src-test/BlockTest.cpp` overrides virtuals on both sides and asserts the effect: the block item's engine fields (max stack size 16, foil), an engine virtual (`isMusicDisk`), the item -> block link (`mBlockType`), and a block hook (`canProvideSupport`). A plain `ICustomItem`'s overrides are only *recorded*, with the reason: its engine object is replaced by the engine in a later pass (measured: the registry holds a different pointer than the one ModAPI built), so its fields read the defaults - a pre-existing ModAPI issue, out of scope here. Note also that this build has no RTTI data, so `dynamic_cast` aborts ("no RTTI data") and a `static_cast` on a pointer you own is the way to go

- Block items now send a client definition of their own: `NetworkTagBuilder` gained a `buildClientComponents(ICustomBlockItem const&)` overload and `ICustomBlockItem` gained `getIcon`, `getDisplayName` and a `buildNetworkTag` override. Without it the creative entry for a block item had no texture (the block itself rendered, so the packs were delivered - the item definition simply had no icon) and the name stayed an untranslated key. Measured after the change: the item tag carries the texture name (`blockItem.tagHasIcon`). The test also names its block item exactly like its block, as vanilla does (`minecraft:stone` is both), which the engine's block -> item resolution for pick block relies on, and its resource pack now ships `texts/en_US.lang` / `texts/zh_CN.lang`

### Fixed

- The vanilla `minecraft:stick` losing its icon on a client (`Missing icon for data-driven item 'minecraft:stick'`) was caused by this repository's own `[REGISTRY]` suite: it exercised the vanilla item setters (`setIcon`, `setDisplayName`, `addTag`, `setFireResistant`, `setRepairItem`, `getAndModifyVanillaNetworkTagInfo`) on `minecraft:stick`, which fills `mModifiedVanillaItems` and makes `VanillaItemDefinitionSendHook` replace that item's `components` in the registry packet. The suite now modifies the item it registers itself (`modapi_test:test_item`) and only reads the vanilla one. Measured: with both mods loaded the stick's definition is no longer touched, and all four suites stay green. It was *not* the block item, the runtime packs, `ItemVersion::DataDriven`, item id collisions or the creative pass - each of those was ruled out by an A/B, several of which were invalid until the manifest explained why: `ModAPI` is `"passive": true` and `test` is its only dependent, so renaming `test` away also unloaded ModAPI (LeviLamina reported "loaded 0 mods"), which is what made "disable the test mod" look like a fix
- Follow-up to that, and the reason it was so damaging: `VanillaItemDefinitionSendHook` used to *replace* an item's whole `components` node with the tag recorded by the setters, and that tag is built from `Item::buildNetworkTag()` - for a data driven vanilla item nearly empty (measured: `minecraft:stick` came out as 37 chars holding only `minecraft:hand_equipped`). So modifying such an item dropped everything it had. The hook now *merges* the recorded changes into what the engine already publishes for the item (top level, and inside `item_properties`), which keeps the data the mod did not touch - the icon of a modified item, for instance

- Re-registering an item name left the replaced item in `ItemRegistry::mItemRegistry`: the new item was appended while the old one stayed in the vector, so every pass that walks it (the client definition build, and this API's own `forEachItemInRegistry`) saw two items answering to one name. The leftover carried none of the values its mod had applied (`max stack size` 64 instead of 16), and the object handed back by the registration differed from the one the registry exposed. Registration now drops the replaced entry, and a custom item's `_init` values survive - verified server side: `plain item fields: stack = 16, glint = true`, and the registry object and the registration product are the same pointer

- `/give <custom item>` silently produced nothing: the enum entry was written as `CommandItem{id, true, false}`, but the type is a union of `{ short mVersion; bool mOverrideAux; int mId; }` over a `uint64`. The id therefore landed in `mVersion` (truncated) and `mId` was left 0. Reading the engine's own `CommandItem::createInstance` settled the rest: it only treats `mId` as the item's own id when `mVersion` is **non-zero** - with 0 it converts the value as a *legacy* id through a different table, which is why an entry for `minecraft:stick` resolved to `minecraft:oak_sign` while our item resolved to nothing. The entry is now `{1, true, id}`. The registry suite builds that value and asserts `createInstance` returns the item (`give.*`)
- Note for whoever tests this next: the item must be reached by a **real** player - a virtual one gives `No targets matched selector`; and an item whose identifier contains a dot (`modapi_test:item.test_block`) is rejected by the command parser outright, so a block item registered under that name cannot be given at all

- The definition packet a client receives was never filled in for custom items. `ModAPI` hooked
  `ItemRegistryPacket::write`, but the old build shows the packet is serialized through
  `writeWithSerializationMode` reaching `serialize<ItemRegistryPacketPayload>::write` directly - `write` is a
  different virtual, so the hook never ran. It is now on the payload constructor, which is where `mItems` is
  produced and which every sending path goes through. Reading `components` out of an entry that has none also
  threw `bad variant access` and aborted the whole merge, so the node is only read after a check
- `ICustomItem` stopped setting `mItemParseVersion = ItemVersion::DataDriven`: it was commented out during an
  experiment and left that way. That flag is what gives an item the definition machinery a client needs, and
  custom items came up blank from then on (`Item <name> requires either an icon atlas or icon texture`)
- Re-registering an item name left the replaced item in `ItemRegistry::mItemRegistry`, and
  `CreativeItemRegistry` appended a creative entry per registration, so items showed up twice in a client's
  inventory and the leftover reported default fields. Registration now drops the replaced entry, and
  `registerCreativeItem` replaces the entry for an item instead of appending another
- `/give <custom item>` silently did nothing: the item enum entry was written as `CommandItem{id, true, false}`,
  but the type is a union of `{ short mVersion; bool mOverrideAux; int mId; }`. `CommandItem::createInstance`
  only reads `mId` as the item's own id when `mVersion` is non-zero - with 0 it converts the value as a *legacy*
  id through a different table
- A block item's id has to be the block's id folded into a short (`id <= 0xFF ? id : 255 - id`), which is what
  `ItemRegistryRef::registerBlockItem<BlockItem>` hands to `BlockItem`'s constructor. Assigning an id afterwards
  left the item's own state disagreeing with `mId`, which showed up as a creative entry nothing could be done
  with
- A block whose type a mod registers in C++ is not listed by the engine at all: the engine creates items for the
  blocks it parses out of documents, so that item is queued for the creative pass instead. The queue cannot
  duplicate anything now that `registerCreativeItem` replaces

- A block item drawn as the block itself (rather than as a flat picture) needs the `minecraft:block_placer`
  component in its definition, with the block it stands for. Two details decide whether a client acts on it, and
  both come out of `PlanterItemComponent::buildNetworkTag` in the old build, which writes `canUseBlockAsIcon` from
  the component's byte +64 and `replaceBlockItem` from +65: the keys are **camelCase** in the network form (a
  *document* uses `replace_block_item`, bound by `bindType`), and `canUseBlockAsIcon` has no document key at all -
  the definition sent to a client is the only place it can be set. A `minecraft:icon` must be left out at the same
  time: `ComponentItem::getIconInfo` reads the icon component first and only falls back to the item's block when
  there is none, so writing an icon pins the item to a texture

- A block registered from C++ now reaches clients without any document. The engine builds the list a client is
  told about (`StartGamePacketPayload::mBlockProperties`) out of the block definition group, and a block whose type
  ModAPI registered itself is not in that group - so the packet's write path adds the entry it is missing. The entry
  is field for field the one the engine writes itself (read out of a live one as SNBT), which is why it works: the
  same `components`/`minecraft:material_instances`, `menu_category`, `molangVersion` and `vanilla_block_data`, with
  the block's own id. `ServerBlockProperty`'s default constructor is declared in the header but not exported, so its
  definition is written here.
- The document path is gone with it: `registerBlockFromMemoryJson`, `registerBlockFromJsonFile`, `deferBlockDocument`
  and the runtime pack they installed were removed, along with the suppression of the engine's block type and the
  bookkeeping that went with it. A block is a `::BlockType` subclass registered under an identifier, and the texture
  a client draws it with is named with `BlockRegistry::setBlockTexture` - the texture file and the resource pack
  mappings (`blocks.json`, `terrain_texture.json`) are the mod's own, exactly as they are for an addon block.
- `ICustomBlock` is gone as well: it only forwarded a constructor to `::BlockType`, so a mod derives from
  `::BlockType` directly. `ICustomFeature` moved to `include/modapi/worldgen/base/` and
  `src/modapi/worldgen/base/`, which is where every other registry keeps its interfaces.

- The definition-level part of a block entry comes out of the registration's own `BlockDescription` instead of
  defaults: `menu_category.category` through the engine's exported
  `SharedTypes::v1_21_110::ItemCategory::stringFromCreativeItemCategory`, `group` and `is_hidden_in_commands` from
  `BlockMenuCategory`, and `vanilla_block_data.block_id` / `material` from `VanillaBlockData` (the material name
  through `BlockEnum::MaterialTypeToString`). `BlockPermutationDescription`s are published as `permutations`, each
  with its `condition` (`ExpressionNode::getExpressionString`), its components and its tags - the same three nodes the
  engine writes.
- A block is registered with what to tell a client about it, in one call and in the engine's own types:
  `registerBlock<MyBlock>("mymod:my_block", { .mArguments = {...}, .mProperty = {...} })`, with `BlockProperty` made
  of `BlockComponentGroupDescription` / `BlockDescription` / `BlockPermutationDescription`. The same property drives
  both sides: its components are injected into the running block type through `initializeComponentFromCode` - the
  route vanilla uses for its own blocks - and serialised for a client through `isNetworkComponent` + `getName` +
  `buildNetworkTag`, which are the calls `generateServerBlockProperties` makes. The entry produced this way matches
  the engine's own field for field (measured against the entry the engine writes for an addon block: identical apart
  from each block's own texture name and id). No `setBlockTexture`/`setBlockProperty`: a property is handed over with
  the registration.
- The engine declares default constructors and copy operations for those definition types and does not export them,
  so `include/modapi/block/base/EngineDefaults.h` supplies the definitions inline - default constructors only, and
  copy/assignment only where the linker proved them missing. A type holding a `CompoundTag`
  (`ServerBlockProperty`) keeps the engine's exported copy: a memberwise `= default` there copies the tag shallowly
  and two objects then free the same storage, which crashed the server at `StartGamePacket` time.

### Changed

- Adapted LeviLamina 26.10.4

### Fixed

- Fixed the signature of `FuckMultipleManifestOutput`

## [0.4.0] - 2026-01-31

- The test ships a real 16x16 PNG for its block texture (embedded, generated with Pillow, also kept at `src-test/assets/modapi_test_block.png`) instead of a placeholder file, and asserts the shipped file carries the PNG signature: a placeholder leaves a client with nothing to draw, which was a real defect spotted in review

### Added

- `modapi::item::ICustomBlockItem` (`include/modapi/item/base/ICustomBlockItem.h`): the item that places a custom block. It derives from the engine's `::BlockItem` and is registered through `ItemRegistry` like any other item (`isEntry` already accepted `std::derived_from<Entry, ::Item>`, so nothing had to be loosened), with an `initCustomItem(ICustomBlockItem&)` overload next to the existing `ICustomItem`/`ICustomArmorItem` ones. Measured: `::BlockItem` does *not* link itself to its block (`getBlockTypeForRendering` is not even overridden by it) - the link is `::Item::mBlockType`, so the class sets it in `_init`; before that an item built for a custom block reported `minecraft:air`, afterwards it reports the block. Registration timing needed no change either: block types are registered right after the level reads its definitions, which is measured to be before vanilla builds its items, so a block item finds its block; an earlier attempt to move block registration into the item pass was reverted, along with the switch to a ModAPI owned id range
- `src-test/BlockTest.cpp` overrides virtuals on both sides and asserts the effect: the block item's engine fields (max stack size 16, foil), an engine virtual (`isMusicDisk`), the item -> block link (`mBlockType`), and a block hook (`canProvideSupport`). A plain `ICustomItem`'s overrides are only *recorded*, with the reason: its engine object is replaced by the engine in a later pass (measured: the registry holds a different pointer than the one ModAPI built), so its fields read the defaults - a pre-existing ModAPI issue, out of scope here. Note also that this build has no RTTI data, so `dynamic_cast` aborts ("no RTTI data") and a `static_cast` on a pointer you own is the way to go

- Block items now send a client definition of their own: `NetworkTagBuilder` gained a `buildClientComponents(ICustomBlockItem const&)` overload and `ICustomBlockItem` gained `getIcon`, `getDisplayName` and a `buildNetworkTag` override. Without it the creative entry for a block item had no texture (the block itself rendered, so the packs were delivered - the item definition simply had no icon) and the name stayed an untranslated key. Measured after the change: the item tag carries the texture name (`blockItem.tagHasIcon`). The test also names its block item exactly like its block, as vanilla does (`minecraft:stone` is both), which the engine's block -> item resolution for pick block relies on, and its resource pack now ships `texts/en_US.lang` / `texts/zh_CN.lang`

### Fixed

- The vanilla `minecraft:stick` losing its icon on a client (`Missing icon for data-driven item 'minecraft:stick'`) was caused by this repository's own `[REGISTRY]` suite: it exercised the vanilla item setters (`setIcon`, `setDisplayName`, `addTag`, `setFireResistant`, `setRepairItem`, `getAndModifyVanillaNetworkTagInfo`) on `minecraft:stick`, which fills `mModifiedVanillaItems` and makes `VanillaItemDefinitionSendHook` replace that item's `components` in the registry packet. The suite now modifies the item it registers itself (`modapi_test:test_item`) and only reads the vanilla one. Measured: with both mods loaded the stick's definition is no longer touched, and all four suites stay green. It was *not* the block item, the runtime packs, `ItemVersion::DataDriven`, item id collisions or the creative pass - each of those was ruled out by an A/B, several of which were invalid until the manifest explained why: `ModAPI` is `"passive": true` and `test` is its only dependent, so renaming `test` away also unloaded ModAPI (LeviLamina reported "loaded 0 mods"), which is what made "disable the test mod" look like a fix
- Follow-up to that, and the reason it was so damaging: `VanillaItemDefinitionSendHook` used to *replace* an item's whole `components` node with the tag recorded by the setters, and that tag is built from `Item::buildNetworkTag()` - for a data driven vanilla item nearly empty (measured: `minecraft:stick` came out as 37 chars holding only `minecraft:hand_equipped`). So modifying such an item dropped everything it had. The hook now *merges* the recorded changes into what the engine already publishes for the item (top level, and inside `item_properties`), which keeps the data the mod did not touch - the icon of a modified item, for instance

- Re-registering an item name left the replaced item in `ItemRegistry::mItemRegistry`: the new item was appended while the old one stayed in the vector, so every pass that walks it (the client definition build, and this API's own `forEachItemInRegistry`) saw two items answering to one name. The leftover carried none of the values its mod had applied (`max stack size` 64 instead of 16), and the object handed back by the registration differed from the one the registry exposed. Registration now drops the replaced entry, and a custom item's `_init` values survive - verified server side: `plain item fields: stack = 16, glint = true`, and the registry object and the registration product are the same pointer

- `/give <custom item>` silently produced nothing: the enum entry was written as `CommandItem{id, true, false}`, but the type is a union of `{ short mVersion; bool mOverrideAux; int mId; }` over a `uint64`. The id therefore landed in `mVersion` (truncated) and `mId` was left 0. Reading the engine's own `CommandItem::createInstance` settled the rest: it only treats `mId` as the item's own id when `mVersion` is **non-zero** - with 0 it converts the value as a *legacy* id through a different table, which is why an entry for `minecraft:stick` resolved to `minecraft:oak_sign` while our item resolved to nothing. The entry is now `{1, true, id}`. The registry suite builds that value and asserts `createInstance` returns the item (`give.*`)
- Note for whoever tests this next: the item must be reached by a **real** player - a virtual one gives `No targets matched selector`; and an item whose identifier contains a dot (`modapi_test:item.test_block`) is rejected by the command parser outright, so a block item registered under that name cannot be given at all

- The definition packet a client receives was never filled in for custom items. `ModAPI` hooked
  `ItemRegistryPacket::write`, but the old build shows the packet is serialized through
  `writeWithSerializationMode` reaching `serialize<ItemRegistryPacketPayload>::write` directly - `write` is a
  different virtual, so the hook never ran. It is now on the payload constructor, which is where `mItems` is
  produced and which every sending path goes through. Reading `components` out of an entry that has none also
  threw `bad variant access` and aborted the whole merge, so the node is only read after a check
- `ICustomItem` stopped setting `mItemParseVersion = ItemVersion::DataDriven`: it was commented out during an
  experiment and left that way. That flag is what gives an item the definition machinery a client needs, and
  custom items came up blank from then on (`Item <name> requires either an icon atlas or icon texture`)
- Re-registering an item name left the replaced item in `ItemRegistry::mItemRegistry`, and
  `CreativeItemRegistry` appended a creative entry per registration, so items showed up twice in a client's
  inventory and the leftover reported default fields. Registration now drops the replaced entry, and
  `registerCreativeItem` replaces the entry for an item instead of appending another
- `/give <custom item>` silently did nothing: the item enum entry was written as `CommandItem{id, true, false}`,
  but the type is a union of `{ short mVersion; bool mOverrideAux; int mId; }`. `CommandItem::createInstance`
  only reads `mId` as the item's own id when `mVersion` is non-zero - with 0 it converts the value as a *legacy*
  id through a different table
- A block item's id has to be the block's id folded into a short (`id <= 0xFF ? id : 255 - id`), which is what
  `ItemRegistryRef::registerBlockItem<BlockItem>` hands to `BlockItem`'s constructor. Assigning an id afterwards
  left the item's own state disagreeing with `mId`, which showed up as a creative entry nothing could be done
  with
- A block whose type a mod registers in C++ is not listed by the engine at all: the engine creates items for the
  blocks it parses out of documents, so that item is queued for the creative pass instead. The queue cannot
  duplicate anything now that `registerCreativeItem` replaces

- A block item drawn as the block itself (rather than as a flat picture) needs the `minecraft:block_placer`
  component in its definition, with the block it stands for. Two details decide whether a client acts on it, and
  both come out of `PlanterItemComponent::buildNetworkTag` in the old build, which writes `canUseBlockAsIcon` from
  the component's byte +64 and `replaceBlockItem` from +65: the keys are **camelCase** in the network form (a
  *document* uses `replace_block_item`, bound by `bindType`), and `canUseBlockAsIcon` has no document key at all -
  the definition sent to a client is the only place it can be set. A `minecraft:icon` must be left out at the same
  time: `ComponentItem::getIconInfo` reads the icon component first and only falls back to the item's block when
  there is none, so writing an icon pins the item to a texture

- A block registered from C++ now reaches clients without any document. The engine builds the list a client is
  told about (`StartGamePacketPayload::mBlockProperties`) out of the block definition group, and a block whose type
  ModAPI registered itself is not in that group - so the packet's write path adds the entry it is missing. The entry
  is field for field the one the engine writes itself (read out of a live one as SNBT), which is why it works: the
  same `components`/`minecraft:material_instances`, `menu_category`, `molangVersion` and `vanilla_block_data`, with
  the block's own id. `ServerBlockProperty`'s default constructor is declared in the header but not exported, so its
  definition is written here.
- The document path is gone with it: `registerBlockFromMemoryJson`, `registerBlockFromJsonFile`, `deferBlockDocument`
  and the runtime pack they installed were removed, along with the suppression of the engine's block type and the
  bookkeeping that went with it. A block is a `::BlockType` subclass registered under an identifier, and the texture
  a client draws it with is named with `BlockRegistry::setBlockTexture` - the texture file and the resource pack
  mappings (`blocks.json`, `terrain_texture.json`) are the mod's own, exactly as they are for an addon block.
- `ICustomBlock` is gone as well: it only forwarded a constructor to `::BlockType`, so a mod derives from
  `::BlockType` directly. `ICustomFeature` moved to `include/modapi/worldgen/base/` and
  `src/modapi/worldgen/base/`, which is where every other registry keeps its interfaces.

- The definition-level part of a block entry comes out of the registration's own `BlockDescription` instead of
  defaults: `menu_category.category` through the engine's exported
  `SharedTypes::v1_21_110::ItemCategory::stringFromCreativeItemCategory`, `group` and `is_hidden_in_commands` from
  `BlockMenuCategory`, and `vanilla_block_data.block_id` / `material` from `VanillaBlockData` (the material name
  through `BlockEnum::MaterialTypeToString`). `BlockPermutationDescription`s are published as `permutations`, each
  with its `condition` (`ExpressionNode::getExpressionString`), its components and its tags - the same three nodes the
  engine writes.
- A block is registered with what to tell a client about it, in one call and in the engine's own types:
  `registerBlock<MyBlock>("mymod:my_block", { .mArguments = {...}, .mProperty = {...} })`, with `BlockProperty` made
  of `BlockComponentGroupDescription` / `BlockDescription` / `BlockPermutationDescription`. The same property drives
  both sides: its components are injected into the running block type through `initializeComponentFromCode` - the
  route vanilla uses for its own blocks - and serialised for a client through `isNetworkComponent` + `getName` +
  `buildNetworkTag`, which are the calls `generateServerBlockProperties` makes. The entry produced this way matches
  the engine's own field for field (measured against the entry the engine writes for an addon block: identical apart
  from each block's own texture name and id). No `setBlockTexture`/`setBlockProperty`: a property is handed over with
  the registration.
- The engine declares default constructors and copy operations for those definition types and does not export them,
  so `include/modapi/block/base/EngineDefaults.h` supplies the definitions inline - default constructors only, and
  copy/assignment only where the linker proved them missing. A type holding a `CompoundTag`
  (`ServerBlockProperty`) keeps the engine's exported copy: a memberwise `= default` there copies the tag shallowly
  and two objects then free the same storage, which crashed the server at `StartGamePacket` time.

### Changed

- `RuntimePack` gets a fresh uuid on every construction instead of a name derived one (and logs it): a client that already has a pack with the same uuid does not download it again, so a pack rebuilt on each server start has to offer an identity no client has seen. Packs also have to be installed while mods load - the stack clients are offered is composed when a level starts, so a pack installed later (from a command, for instance) never reaches them; the test installs its packs there now

- The test ships a real 16x16 PNG for its block texture (embedded, generated with Pillow, also kept at `src-test/assets/modapi_test_block.png`) instead of a placeholder file, and asserts the shipped file carries the PNG signature: a placeholder leaves a client with nothing to draw, which was a real defect spotted in review

### Added

- `modapi::item::ICustomBlockItem` (`include/modapi/item/base/ICustomBlockItem.h`): the item that places a custom block. It derives from the engine's `::BlockItem` and is registered through `ItemRegistry` like any other item (`isEntry` already accepted `std::derived_from<Entry, ::Item>`, so nothing had to be loosened), with an `initCustomItem(ICustomBlockItem&)` overload next to the existing `ICustomItem`/`ICustomArmorItem` ones. Measured: `::BlockItem` does *not* link itself to its block (`getBlockTypeForRendering` is not even overridden by it) - the link is `::Item::mBlockType`, so the class sets it in `_init`; before that an item built for a custom block reported `minecraft:air`, afterwards it reports the block. Registration timing needed no change either: block types are registered right after the level reads its definitions, which is measured to be before vanilla builds its items, so a block item finds its block; an earlier attempt to move block registration into the item pass was reverted, along with the switch to a ModAPI owned id range
- `src-test/BlockTest.cpp` overrides virtuals on both sides and asserts the effect: the block item's engine fields (max stack size 16, foil), an engine virtual (`isMusicDisk`), the item -> block link (`mBlockType`), and a block hook (`canProvideSupport`). A plain `ICustomItem`'s overrides are only *recorded*, with the reason: its engine object is replaced by the engine in a later pass (measured: the registry holds a different pointer than the one ModAPI built), so its fields read the defaults - a pre-existing ModAPI issue, out of scope here. Note also that this build has no RTTI data, so `dynamic_cast` aborts ("no RTTI data") and a `static_cast` on a pointer you own is the way to go

- Block items now send a client definition of their own: `NetworkTagBuilder` gained a `buildClientComponents(ICustomBlockItem const&)` overload and `ICustomBlockItem` gained `getIcon`, `getDisplayName` and a `buildNetworkTag` override. Without it the creative entry for a block item had no texture (the block itself rendered, so the packs were delivered - the item definition simply had no icon) and the name stayed an untranslated key. Measured after the change: the item tag carries the texture name (`blockItem.tagHasIcon`). The test also names its block item exactly like its block, as vanilla does (`minecraft:stone` is both), which the engine's block -> item resolution for pick block relies on, and its resource pack now ships `texts/en_US.lang` / `texts/zh_CN.lang`

### Fixed

- The vanilla `minecraft:stick` losing its icon on a client (`Missing icon for data-driven item 'minecraft:stick'`) was caused by this repository's own `[REGISTRY]` suite: it exercised the vanilla item setters (`setIcon`, `setDisplayName`, `addTag`, `setFireResistant`, `setRepairItem`, `getAndModifyVanillaNetworkTagInfo`) on `minecraft:stick`, which fills `mModifiedVanillaItems` and makes `VanillaItemDefinitionSendHook` replace that item's `components` in the registry packet. The suite now modifies the item it registers itself (`modapi_test:test_item`) and only reads the vanilla one. Measured: with both mods loaded the stick's definition is no longer touched, and all four suites stay green. It was *not* the block item, the runtime packs, `ItemVersion::DataDriven`, item id collisions or the creative pass - each of those was ruled out by an A/B, several of which were invalid until the manifest explained why: `ModAPI` is `"passive": true` and `test` is its only dependent, so renaming `test` away also unloaded ModAPI (LeviLamina reported "loaded 0 mods"), which is what made "disable the test mod" look like a fix
- Follow-up to that, and the reason it was so damaging: `VanillaItemDefinitionSendHook` used to *replace* an item's whole `components` node with the tag recorded by the setters, and that tag is built from `Item::buildNetworkTag()` - for a data driven vanilla item nearly empty (measured: `minecraft:stick` came out as 37 chars holding only `minecraft:hand_equipped`). So modifying such an item dropped everything it had. The hook now *merges* the recorded changes into what the engine already publishes for the item (top level, and inside `item_properties`), which keeps the data the mod did not touch - the icon of a modified item, for instance

- Re-registering an item name left the replaced item in `ItemRegistry::mItemRegistry`: the new item was appended while the old one stayed in the vector, so every pass that walks it (the client definition build, and this API's own `forEachItemInRegistry`) saw two items answering to one name. The leftover carried none of the values its mod had applied (`max stack size` 64 instead of 16), and the object handed back by the registration differed from the one the registry exposed. Registration now drops the replaced entry, and a custom item's `_init` values survive - verified server side: `plain item fields: stack = 16, glint = true`, and the registry object and the registration product are the same pointer

- `/give <custom item>` silently produced nothing: the enum entry was written as `CommandItem{id, true, false}`, but the type is a union of `{ short mVersion; bool mOverrideAux; int mId; }` over a `uint64`. The id therefore landed in `mVersion` (truncated) and `mId` was left 0. Reading the engine's own `CommandItem::createInstance` settled the rest: it only treats `mId` as the item's own id when `mVersion` is **non-zero** - with 0 it converts the value as a *legacy* id through a different table, which is why an entry for `minecraft:stick` resolved to `minecraft:oak_sign` while our item resolved to nothing. The entry is now `{1, true, id}`. The registry suite builds that value and asserts `createInstance` returns the item (`give.*`)
- Note for whoever tests this next: the item must be reached by a **real** player - a virtual one gives `No targets matched selector`; and an item whose identifier contains a dot (`modapi_test:item.test_block`) is rejected by the command parser outright, so a block item registered under that name cannot be given at all

- The definition packet a client receives was never filled in for custom items. `ModAPI` hooked
  `ItemRegistryPacket::write`, but the old build shows the packet is serialized through
  `writeWithSerializationMode` reaching `serialize<ItemRegistryPacketPayload>::write` directly - `write` is a
  different virtual, so the hook never ran. It is now on the payload constructor, which is where `mItems` is
  produced and which every sending path goes through. Reading `components` out of an entry that has none also
  threw `bad variant access` and aborted the whole merge, so the node is only read after a check
- `ICustomItem` stopped setting `mItemParseVersion = ItemVersion::DataDriven`: it was commented out during an
  experiment and left that way. That flag is what gives an item the definition machinery a client needs, and
  custom items came up blank from then on (`Item <name> requires either an icon atlas or icon texture`)
- Re-registering an item name left the replaced item in `ItemRegistry::mItemRegistry`, and
  `CreativeItemRegistry` appended a creative entry per registration, so items showed up twice in a client's
  inventory and the leftover reported default fields. Registration now drops the replaced entry, and
  `registerCreativeItem` replaces the entry for an item instead of appending another
- `/give <custom item>` silently did nothing: the item enum entry was written as `CommandItem{id, true, false}`,
  but the type is a union of `{ short mVersion; bool mOverrideAux; int mId; }`. `CommandItem::createInstance`
  only reads `mId` as the item's own id when `mVersion` is non-zero - with 0 it converts the value as a *legacy*
  id through a different table
- A block item's id has to be the block's id folded into a short (`id <= 0xFF ? id : 255 - id`), which is what
  `ItemRegistryRef::registerBlockItem<BlockItem>` hands to `BlockItem`'s constructor. Assigning an id afterwards
  left the item's own state disagreeing with `mId`, which showed up as a creative entry nothing could be done
  with
- A block whose type a mod registers in C++ is not listed by the engine at all: the engine creates items for the
  blocks it parses out of documents, so that item is queued for the creative pass instead. The queue cannot
  duplicate anything now that `registerCreativeItem` replaces

- A block item drawn as the block itself (rather than as a flat picture) needs the `minecraft:block_placer`
  component in its definition, with the block it stands for. Two details decide whether a client acts on it, and
  both come out of `PlanterItemComponent::buildNetworkTag` in the old build, which writes `canUseBlockAsIcon` from
  the component's byte +64 and `replaceBlockItem` from +65: the keys are **camelCase** in the network form (a
  *document* uses `replace_block_item`, bound by `bindType`), and `canUseBlockAsIcon` has no document key at all -
  the definition sent to a client is the only place it can be set. A `minecraft:icon` must be left out at the same
  time: `ComponentItem::getIconInfo` reads the icon component first and only falls back to the item's block when
  there is none, so writing an icon pins the item to a texture

- A block registered from C++ now reaches clients without any document. The engine builds the list a client is
  told about (`StartGamePacketPayload::mBlockProperties`) out of the block definition group, and a block whose type
  ModAPI registered itself is not in that group - so the packet's write path adds the entry it is missing. The entry
  is field for field the one the engine writes itself (read out of a live one as SNBT), which is why it works: the
  same `components`/`minecraft:material_instances`, `menu_category`, `molangVersion` and `vanilla_block_data`, with
  the block's own id. `ServerBlockProperty`'s default constructor is declared in the header but not exported, so its
  definition is written here.
- The document path is gone with it: `registerBlockFromMemoryJson`, `registerBlockFromJsonFile`, `deferBlockDocument`
  and the runtime pack they installed were removed, along with the suppression of the engine's block type and the
  bookkeeping that went with it. A block is a `::BlockType` subclass registered under an identifier, and the texture
  a client draws it with is named with `BlockRegistry::setBlockTexture` - the texture file and the resource pack
  mappings (`blocks.json`, `terrain_texture.json`) are the mod's own, exactly as they are for an addon block.
- `ICustomBlock` is gone as well: it only forwarded a constructor to `::BlockType`, so a mod derives from
  `::BlockType` directly. `ICustomFeature` moved to `include/modapi/worldgen/base/` and
  `src/modapi/worldgen/base/`, which is where every other registry keeps its interfaces.

- The definition-level part of a block entry comes out of the registration's own `BlockDescription` instead of
  defaults: `menu_category.category` through the engine's exported
  `SharedTypes::v1_21_110::ItemCategory::stringFromCreativeItemCategory`, `group` and `is_hidden_in_commands` from
  `BlockMenuCategory`, and `vanilla_block_data.block_id` / `material` from `VanillaBlockData` (the material name
  through `BlockEnum::MaterialTypeToString`). `BlockPermutationDescription`s are published as `permutations`, each
  with its `condition` (`ExpressionNode::getExpressionString`), its components and its tags - the same three nodes the
  engine writes.
- A block is registered with what to tell a client about it, in one call and in the engine's own types:
  `registerBlock<MyBlock>("mymod:my_block", { .mArguments = {...}, .mProperty = {...} })`, with `BlockProperty` made
  of `BlockComponentGroupDescription` / `BlockDescription` / `BlockPermutationDescription`. The same property drives
  both sides: its components are injected into the running block type through `initializeComponentFromCode` - the
  route vanilla uses for its own blocks - and serialised for a client through `isNetworkComponent` + `getName` +
  `buildNetworkTag`, which are the calls `generateServerBlockProperties` makes. The entry produced this way matches
  the engine's own field for field (measured against the entry the engine writes for an addon block: identical apart
  from each block's own texture name and id). No `setBlockTexture`/`setBlockProperty`: a property is handed over with
  the registration.
- The engine declares default constructors and copy operations for those definition types and does not export them,
  so `include/modapi/block/base/EngineDefaults.h` supplies the definitions inline - default constructors only, and
  copy/assignment only where the linker proved them missing. A type holding a `CompoundTag`
  (`ServerBlockProperty`) keeps the engine's exported copy: a memberwise `= default` there copies the tag shallowly
  and two objects then free the same storage, which crashed the server at `StartGamePacket` time.

### Changed

- Adapted LeviLamina 1.9.2 and BDS 1.21.132 @killcerr @n15421
- Clean tmpfix for initServer and initclient @n15421

## [0.3.1] - 2025-11-28

- The test ships a real 16x16 PNG for its block texture (embedded, generated with Pillow, also kept at `src-test/assets/modapi_test_block.png`) instead of a placeholder file, and asserts the shipped file carries the PNG signature: a placeholder leaves a client with nothing to draw, which was a real defect spotted in review

### Added

- `modapi::item::ICustomBlockItem` (`include/modapi/item/base/ICustomBlockItem.h`): the item that places a custom block. It derives from the engine's `::BlockItem` and is registered through `ItemRegistry` like any other item (`isEntry` already accepted `std::derived_from<Entry, ::Item>`, so nothing had to be loosened), with an `initCustomItem(ICustomBlockItem&)` overload next to the existing `ICustomItem`/`ICustomArmorItem` ones. Measured: `::BlockItem` does *not* link itself to its block (`getBlockTypeForRendering` is not even overridden by it) - the link is `::Item::mBlockType`, so the class sets it in `_init`; before that an item built for a custom block reported `minecraft:air`, afterwards it reports the block. Registration timing needed no change either: block types are registered right after the level reads its definitions, which is measured to be before vanilla builds its items, so a block item finds its block; an earlier attempt to move block registration into the item pass was reverted, along with the switch to a ModAPI owned id range
- `src-test/BlockTest.cpp` overrides virtuals on both sides and asserts the effect: the block item's engine fields (max stack size 16, foil), an engine virtual (`isMusicDisk`), the item -> block link (`mBlockType`), and a block hook (`canProvideSupport`). A plain `ICustomItem`'s overrides are only *recorded*, with the reason: its engine object is replaced by the engine in a later pass (measured: the registry holds a different pointer than the one ModAPI built), so its fields read the defaults - a pre-existing ModAPI issue, out of scope here. Note also that this build has no RTTI data, so `dynamic_cast` aborts ("no RTTI data") and a `static_cast` on a pointer you own is the way to go

- Block items now send a client definition of their own: `NetworkTagBuilder` gained a `buildClientComponents(ICustomBlockItem const&)` overload and `ICustomBlockItem` gained `getIcon`, `getDisplayName` and a `buildNetworkTag` override. Without it the creative entry for a block item had no texture (the block itself rendered, so the packs were delivered - the item definition simply had no icon) and the name stayed an untranslated key. Measured after the change: the item tag carries the texture name (`blockItem.tagHasIcon`). The test also names its block item exactly like its block, as vanilla does (`minecraft:stone` is both), which the engine's block -> item resolution for pick block relies on, and its resource pack now ships `texts/en_US.lang` / `texts/zh_CN.lang`

### Fixed

- The vanilla `minecraft:stick` losing its icon on a client (`Missing icon for data-driven item 'minecraft:stick'`) was caused by this repository's own `[REGISTRY]` suite: it exercised the vanilla item setters (`setIcon`, `setDisplayName`, `addTag`, `setFireResistant`, `setRepairItem`, `getAndModifyVanillaNetworkTagInfo`) on `minecraft:stick`, which fills `mModifiedVanillaItems` and makes `VanillaItemDefinitionSendHook` replace that item's `components` in the registry packet. The suite now modifies the item it registers itself (`modapi_test:test_item`) and only reads the vanilla one. Measured: with both mods loaded the stick's definition is no longer touched, and all four suites stay green. It was *not* the block item, the runtime packs, `ItemVersion::DataDriven`, item id collisions or the creative pass - each of those was ruled out by an A/B, several of which were invalid until the manifest explained why: `ModAPI` is `"passive": true` and `test` is its only dependent, so renaming `test` away also unloaded ModAPI (LeviLamina reported "loaded 0 mods"), which is what made "disable the test mod" look like a fix
- Follow-up to that, and the reason it was so damaging: `VanillaItemDefinitionSendHook` used to *replace* an item's whole `components` node with the tag recorded by the setters, and that tag is built from `Item::buildNetworkTag()` - for a data driven vanilla item nearly empty (measured: `minecraft:stick` came out as 37 chars holding only `minecraft:hand_equipped`). So modifying such an item dropped everything it had. The hook now *merges* the recorded changes into what the engine already publishes for the item (top level, and inside `item_properties`), which keeps the data the mod did not touch - the icon of a modified item, for instance

- Re-registering an item name left the replaced item in `ItemRegistry::mItemRegistry`: the new item was appended while the old one stayed in the vector, so every pass that walks it (the client definition build, and this API's own `forEachItemInRegistry`) saw two items answering to one name. The leftover carried none of the values its mod had applied (`max stack size` 64 instead of 16), and the object handed back by the registration differed from the one the registry exposed. Registration now drops the replaced entry, and a custom item's `_init` values survive - verified server side: `plain item fields: stack = 16, glint = true`, and the registry object and the registration product are the same pointer

- `/give <custom item>` silently produced nothing: the enum entry was written as `CommandItem{id, true, false}`, but the type is a union of `{ short mVersion; bool mOverrideAux; int mId; }` over a `uint64`. The id therefore landed in `mVersion` (truncated) and `mId` was left 0. Reading the engine's own `CommandItem::createInstance` settled the rest: it only treats `mId` as the item's own id when `mVersion` is **non-zero** - with 0 it converts the value as a *legacy* id through a different table, which is why an entry for `minecraft:stick` resolved to `minecraft:oak_sign` while our item resolved to nothing. The entry is now `{1, true, id}`. The registry suite builds that value and asserts `createInstance` returns the item (`give.*`)
- Note for whoever tests this next: the item must be reached by a **real** player - a virtual one gives `No targets matched selector`; and an item whose identifier contains a dot (`modapi_test:item.test_block`) is rejected by the command parser outright, so a block item registered under that name cannot be given at all

- The definition packet a client receives was never filled in for custom items. `ModAPI` hooked
  `ItemRegistryPacket::write`, but the old build shows the packet is serialized through
  `writeWithSerializationMode` reaching `serialize<ItemRegistryPacketPayload>::write` directly - `write` is a
  different virtual, so the hook never ran. It is now on the payload constructor, which is where `mItems` is
  produced and which every sending path goes through. Reading `components` out of an entry that has none also
  threw `bad variant access` and aborted the whole merge, so the node is only read after a check
- `ICustomItem` stopped setting `mItemParseVersion = ItemVersion::DataDriven`: it was commented out during an
  experiment and left that way. That flag is what gives an item the definition machinery a client needs, and
  custom items came up blank from then on (`Item <name> requires either an icon atlas or icon texture`)
- Re-registering an item name left the replaced item in `ItemRegistry::mItemRegistry`, and
  `CreativeItemRegistry` appended a creative entry per registration, so items showed up twice in a client's
  inventory and the leftover reported default fields. Registration now drops the replaced entry, and
  `registerCreativeItem` replaces the entry for an item instead of appending another
- `/give <custom item>` silently did nothing: the item enum entry was written as `CommandItem{id, true, false}`,
  but the type is a union of `{ short mVersion; bool mOverrideAux; int mId; }`. `CommandItem::createInstance`
  only reads `mId` as the item's own id when `mVersion` is non-zero - with 0 it converts the value as a *legacy*
  id through a different table
- A block item's id has to be the block's id folded into a short (`id <= 0xFF ? id : 255 - id`), which is what
  `ItemRegistryRef::registerBlockItem<BlockItem>` hands to `BlockItem`'s constructor. Assigning an id afterwards
  left the item's own state disagreeing with `mId`, which showed up as a creative entry nothing could be done
  with
- A block whose type a mod registers in C++ is not listed by the engine at all: the engine creates items for the
  blocks it parses out of documents, so that item is queued for the creative pass instead. The queue cannot
  duplicate anything now that `registerCreativeItem` replaces

- A block item drawn as the block itself (rather than as a flat picture) needs the `minecraft:block_placer`
  component in its definition, with the block it stands for. Two details decide whether a client acts on it, and
  both come out of `PlanterItemComponent::buildNetworkTag` in the old build, which writes `canUseBlockAsIcon` from
  the component's byte +64 and `replaceBlockItem` from +65: the keys are **camelCase** in the network form (a
  *document* uses `replace_block_item`, bound by `bindType`), and `canUseBlockAsIcon` has no document key at all -
  the definition sent to a client is the only place it can be set. A `minecraft:icon` must be left out at the same
  time: `ComponentItem::getIconInfo` reads the icon component first and only falls back to the item's block when
  there is none, so writing an icon pins the item to a texture

- A block registered from C++ now reaches clients without any document. The engine builds the list a client is
  told about (`StartGamePacketPayload::mBlockProperties`) out of the block definition group, and a block whose type
  ModAPI registered itself is not in that group - so the packet's write path adds the entry it is missing. The entry
  is field for field the one the engine writes itself (read out of a live one as SNBT), which is why it works: the
  same `components`/`minecraft:material_instances`, `menu_category`, `molangVersion` and `vanilla_block_data`, with
  the block's own id. `ServerBlockProperty`'s default constructor is declared in the header but not exported, so its
  definition is written here.
- The document path is gone with it: `registerBlockFromMemoryJson`, `registerBlockFromJsonFile`, `deferBlockDocument`
  and the runtime pack they installed were removed, along with the suppression of the engine's block type and the
  bookkeeping that went with it. A block is a `::BlockType` subclass registered under an identifier, and the texture
  a client draws it with is named with `BlockRegistry::setBlockTexture` - the texture file and the resource pack
  mappings (`blocks.json`, `terrain_texture.json`) are the mod's own, exactly as they are for an addon block.
- `ICustomBlock` is gone as well: it only forwarded a constructor to `::BlockType`, so a mod derives from
  `::BlockType` directly. `ICustomFeature` moved to `include/modapi/worldgen/base/` and
  `src/modapi/worldgen/base/`, which is where every other registry keeps its interfaces.

- The definition-level part of a block entry comes out of the registration's own `BlockDescription` instead of
  defaults: `menu_category.category` through the engine's exported
  `SharedTypes::v1_21_110::ItemCategory::stringFromCreativeItemCategory`, `group` and `is_hidden_in_commands` from
  `BlockMenuCategory`, and `vanilla_block_data.block_id` / `material` from `VanillaBlockData` (the material name
  through `BlockEnum::MaterialTypeToString`). `BlockPermutationDescription`s are published as `permutations`, each
  with its `condition` (`ExpressionNode::getExpressionString`), its components and its tags - the same three nodes the
  engine writes.
- A block is registered with what to tell a client about it, in one call and in the engine's own types:
  `registerBlock<MyBlock>("mymod:my_block", { .mArguments = {...}, .mProperty = {...} })`, with `BlockProperty` made
  of `BlockComponentGroupDescription` / `BlockDescription` / `BlockPermutationDescription`. The same property drives
  both sides: its components are injected into the running block type through `initializeComponentFromCode` - the
  route vanilla uses for its own blocks - and serialised for a client through `isNetworkComponent` + `getName` +
  `buildNetworkTag`, which are the calls `generateServerBlockProperties` makes. The entry produced this way matches
  the engine's own field for field (measured against the entry the engine writes for an addon block: identical apart
  from each block's own texture name and id). No `setBlockTexture`/`setBlockProperty`: a property is handed over with
  the registration.
- The engine declares default constructors and copy operations for those definition types and does not export them,
  so `include/modapi/block/base/EngineDefaults.h` supplies the definitions inline - default constructors only, and
  copy/assignment only where the linker proved them missing. A type holding a `CompoundTag`
  (`ServerBlockProperty`) keeps the engine's exported copy: a memberwise `= default` there copies the tag shallowly
  and two objects then free the same storage, which crashed the server at `StartGamePacket` time.

### Changed

- `RuntimePack` gets a fresh uuid on every construction instead of a name derived one (and logs it): a client that already has a pack with the same uuid does not download it again, so a pack rebuilt on each server start has to offer an identity no client has seen. Packs also have to be installed while mods load - the stack clients are offered is composed when a level starts, so a pack installed later (from a command, for instance) never reaches them; the test installs its packs there now

- The test ships a real 16x16 PNG for its block texture (embedded, generated with Pillow, also kept at `src-test/assets/modapi_test_block.png`) instead of a placeholder file, and asserts the shipped file carries the PNG signature: a placeholder leaves a client with nothing to draw, which was a real defect spotted in review

### Added

- `modapi::item::ICustomBlockItem` (`include/modapi/item/base/ICustomBlockItem.h`): the item that places a custom block. It derives from the engine's `::BlockItem` and is registered through `ItemRegistry` like any other item (`isEntry` already accepted `std::derived_from<Entry, ::Item>`, so nothing had to be loosened), with an `initCustomItem(ICustomBlockItem&)` overload next to the existing `ICustomItem`/`ICustomArmorItem` ones. Measured: `::BlockItem` does *not* link itself to its block (`getBlockTypeForRendering` is not even overridden by it) - the link is `::Item::mBlockType`, so the class sets it in `_init`; before that an item built for a custom block reported `minecraft:air`, afterwards it reports the block. Registration timing needed no change either: block types are registered right after the level reads its definitions, which is measured to be before vanilla builds its items, so a block item finds its block; an earlier attempt to move block registration into the item pass was reverted, along with the switch to a ModAPI owned id range
- `src-test/BlockTest.cpp` overrides virtuals on both sides and asserts the effect: the block item's engine fields (max stack size 16, foil), an engine virtual (`isMusicDisk`), the item -> block link (`mBlockType`), and a block hook (`canProvideSupport`). A plain `ICustomItem`'s overrides are only *recorded*, with the reason: its engine object is replaced by the engine in a later pass (measured: the registry holds a different pointer than the one ModAPI built), so its fields read the defaults - a pre-existing ModAPI issue, out of scope here. Note also that this build has no RTTI data, so `dynamic_cast` aborts ("no RTTI data") and a `static_cast` on a pointer you own is the way to go

- Block items now send a client definition of their own: `NetworkTagBuilder` gained a `buildClientComponents(ICustomBlockItem const&)` overload and `ICustomBlockItem` gained `getIcon`, `getDisplayName` and a `buildNetworkTag` override. Without it the creative entry for a block item had no texture (the block itself rendered, so the packs were delivered - the item definition simply had no icon) and the name stayed an untranslated key. Measured after the change: the item tag carries the texture name (`blockItem.tagHasIcon`). The test also names its block item exactly like its block, as vanilla does (`minecraft:stone` is both), which the engine's block -> item resolution for pick block relies on, and its resource pack now ships `texts/en_US.lang` / `texts/zh_CN.lang`

### Fixed

- The vanilla `minecraft:stick` losing its icon on a client (`Missing icon for data-driven item 'minecraft:stick'`) was caused by this repository's own `[REGISTRY]` suite: it exercised the vanilla item setters (`setIcon`, `setDisplayName`, `addTag`, `setFireResistant`, `setRepairItem`, `getAndModifyVanillaNetworkTagInfo`) on `minecraft:stick`, which fills `mModifiedVanillaItems` and makes `VanillaItemDefinitionSendHook` replace that item's `components` in the registry packet. The suite now modifies the item it registers itself (`modapi_test:test_item`) and only reads the vanilla one. Measured: with both mods loaded the stick's definition is no longer touched, and all four suites stay green. It was *not* the block item, the runtime packs, `ItemVersion::DataDriven`, item id collisions or the creative pass - each of those was ruled out by an A/B, several of which were invalid until the manifest explained why: `ModAPI` is `"passive": true` and `test` is its only dependent, so renaming `test` away also unloaded ModAPI (LeviLamina reported "loaded 0 mods"), which is what made "disable the test mod" look like a fix
- Follow-up to that, and the reason it was so damaging: `VanillaItemDefinitionSendHook` used to *replace* an item's whole `components` node with the tag recorded by the setters, and that tag is built from `Item::buildNetworkTag()` - for a data driven vanilla item nearly empty (measured: `minecraft:stick` came out as 37 chars holding only `minecraft:hand_equipped`). So modifying such an item dropped everything it had. The hook now *merges* the recorded changes into what the engine already publishes for the item (top level, and inside `item_properties`), which keeps the data the mod did not touch - the icon of a modified item, for instance

- Re-registering an item name left the replaced item in `ItemRegistry::mItemRegistry`: the new item was appended while the old one stayed in the vector, so every pass that walks it (the client definition build, and this API's own `forEachItemInRegistry`) saw two items answering to one name. The leftover carried none of the values its mod had applied (`max stack size` 64 instead of 16), and the object handed back by the registration differed from the one the registry exposed. Registration now drops the replaced entry, and a custom item's `_init` values survive - verified server side: `plain item fields: stack = 16, glint = true`, and the registry object and the registration product are the same pointer

- `/give <custom item>` silently produced nothing: the enum entry was written as `CommandItem{id, true, false}`, but the type is a union of `{ short mVersion; bool mOverrideAux; int mId; }` over a `uint64`. The id therefore landed in `mVersion` (truncated) and `mId` was left 0. Reading the engine's own `CommandItem::createInstance` settled the rest: it only treats `mId` as the item's own id when `mVersion` is **non-zero** - with 0 it converts the value as a *legacy* id through a different table, which is why an entry for `minecraft:stick` resolved to `minecraft:oak_sign` while our item resolved to nothing. The entry is now `{1, true, id}`. The registry suite builds that value and asserts `createInstance` returns the item (`give.*`)
- Note for whoever tests this next: the item must be reached by a **real** player - a virtual one gives `No targets matched selector`; and an item whose identifier contains a dot (`modapi_test:item.test_block`) is rejected by the command parser outright, so a block item registered under that name cannot be given at all

- The definition packet a client receives was never filled in for custom items. `ModAPI` hooked
  `ItemRegistryPacket::write`, but the old build shows the packet is serialized through
  `writeWithSerializationMode` reaching `serialize<ItemRegistryPacketPayload>::write` directly - `write` is a
  different virtual, so the hook never ran. It is now on the payload constructor, which is where `mItems` is
  produced and which every sending path goes through. Reading `components` out of an entry that has none also
  threw `bad variant access` and aborted the whole merge, so the node is only read after a check
- `ICustomItem` stopped setting `mItemParseVersion = ItemVersion::DataDriven`: it was commented out during an
  experiment and left that way. That flag is what gives an item the definition machinery a client needs, and
  custom items came up blank from then on (`Item <name> requires either an icon atlas or icon texture`)
- Re-registering an item name left the replaced item in `ItemRegistry::mItemRegistry`, and
  `CreativeItemRegistry` appended a creative entry per registration, so items showed up twice in a client's
  inventory and the leftover reported default fields. Registration now drops the replaced entry, and
  `registerCreativeItem` replaces the entry for an item instead of appending another
- `/give <custom item>` silently did nothing: the item enum entry was written as `CommandItem{id, true, false}`,
  but the type is a union of `{ short mVersion; bool mOverrideAux; int mId; }`. `CommandItem::createInstance`
  only reads `mId` as the item's own id when `mVersion` is non-zero - with 0 it converts the value as a *legacy*
  id through a different table
- A block item's id has to be the block's id folded into a short (`id <= 0xFF ? id : 255 - id`), which is what
  `ItemRegistryRef::registerBlockItem<BlockItem>` hands to `BlockItem`'s constructor. Assigning an id afterwards
  left the item's own state disagreeing with `mId`, which showed up as a creative entry nothing could be done
  with
- A block whose type a mod registers in C++ is not listed by the engine at all: the engine creates items for the
  blocks it parses out of documents, so that item is queued for the creative pass instead. The queue cannot
  duplicate anything now that `registerCreativeItem` replaces

- A block item drawn as the block itself (rather than as a flat picture) needs the `minecraft:block_placer`
  component in its definition, with the block it stands for. Two details decide whether a client acts on it, and
  both come out of `PlanterItemComponent::buildNetworkTag` in the old build, which writes `canUseBlockAsIcon` from
  the component's byte +64 and `replaceBlockItem` from +65: the keys are **camelCase** in the network form (a
  *document* uses `replace_block_item`, bound by `bindType`), and `canUseBlockAsIcon` has no document key at all -
  the definition sent to a client is the only place it can be set. A `minecraft:icon` must be left out at the same
  time: `ComponentItem::getIconInfo` reads the icon component first and only falls back to the item's block when
  there is none, so writing an icon pins the item to a texture

- A block registered from C++ now reaches clients without any document. The engine builds the list a client is
  told about (`StartGamePacketPayload::mBlockProperties`) out of the block definition group, and a block whose type
  ModAPI registered itself is not in that group - so the packet's write path adds the entry it is missing. The entry
  is field for field the one the engine writes itself (read out of a live one as SNBT), which is why it works: the
  same `components`/`minecraft:material_instances`, `menu_category`, `molangVersion` and `vanilla_block_data`, with
  the block's own id. `ServerBlockProperty`'s default constructor is declared in the header but not exported, so its
  definition is written here.
- The document path is gone with it: `registerBlockFromMemoryJson`, `registerBlockFromJsonFile`, `deferBlockDocument`
  and the runtime pack they installed were removed, along with the suppression of the engine's block type and the
  bookkeeping that went with it. A block is a `::BlockType` subclass registered under an identifier, and the texture
  a client draws it with is named with `BlockRegistry::setBlockTexture` - the texture file and the resource pack
  mappings (`blocks.json`, `terrain_texture.json`) are the mod's own, exactly as they are for an addon block.
- `ICustomBlock` is gone as well: it only forwarded a constructor to `::BlockType`, so a mod derives from
  `::BlockType` directly. `ICustomFeature` moved to `include/modapi/worldgen/base/` and
  `src/modapi/worldgen/base/`, which is where every other registry keeps its interfaces.

- The definition-level part of a block entry comes out of the registration's own `BlockDescription` instead of
  defaults: `menu_category.category` through the engine's exported
  `SharedTypes::v1_21_110::ItemCategory::stringFromCreativeItemCategory`, `group` and `is_hidden_in_commands` from
  `BlockMenuCategory`, and `vanilla_block_data.block_id` / `material` from `VanillaBlockData` (the material name
  through `BlockEnum::MaterialTypeToString`). `BlockPermutationDescription`s are published as `permutations`, each
  with its `condition` (`ExpressionNode::getExpressionString`), its components and its tags - the same three nodes the
  engine writes.
- A block is registered with what to tell a client about it, in one call and in the engine's own types:
  `registerBlock<MyBlock>("mymod:my_block", { .mArguments = {...}, .mProperty = {...} })`, with `BlockProperty` made
  of `BlockComponentGroupDescription` / `BlockDescription` / `BlockPermutationDescription`. The same property drives
  both sides: its components are injected into the running block type through `initializeComponentFromCode` - the
  route vanilla uses for its own blocks - and serialised for a client through `isNetworkComponent` + `getName` +
  `buildNetworkTag`, which are the calls `generateServerBlockProperties` makes. The entry produced this way matches
  the engine's own field for field (measured against the entry the engine writes for an addon block: identical apart
  from each block's own texture name and id). No `setBlockTexture`/`setBlockProperty`: a property is handed over with
  the registration.
- The engine declares default constructors and copy operations for those definition types and does not export them,
  so `include/modapi/block/base/EngineDefaults.h` supplies the definitions inline - default constructors only, and
  copy/assignment only where the linker proved them missing. A type holding a `CompoundTag`
  (`ServerBlockProperty`) keeps the engine's exported copy: a memberwise `= default` there copies the tag shallowly
  and two objects then free the same storage, which crashed the server at `StartGamePacket` time.

### Changed

- Adapted LeviLamina 1.7.6 and BDS 1.21.124 @zimuya4153
- Correct the name of the macro definition @zimuya4153

### Fixed

- Fixed the HumanoidArmorItem link issue temporarily @zimuya4153

## [0.3.0] - 2025-11-07

- The test ships a real 16x16 PNG for its block texture (embedded, generated with Pillow, also kept at `src-test/assets/modapi_test_block.png`) instead of a placeholder file, and asserts the shipped file carries the PNG signature: a placeholder leaves a client with nothing to draw, which was a real defect spotted in review

### Added

- `modapi::item::ICustomBlockItem` (`include/modapi/item/base/ICustomBlockItem.h`): the item that places a custom block. It derives from the engine's `::BlockItem` and is registered through `ItemRegistry` like any other item (`isEntry` already accepted `std::derived_from<Entry, ::Item>`, so nothing had to be loosened), with an `initCustomItem(ICustomBlockItem&)` overload next to the existing `ICustomItem`/`ICustomArmorItem` ones. Measured: `::BlockItem` does *not* link itself to its block (`getBlockTypeForRendering` is not even overridden by it) - the link is `::Item::mBlockType`, so the class sets it in `_init`; before that an item built for a custom block reported `minecraft:air`, afterwards it reports the block. Registration timing needed no change either: block types are registered right after the level reads its definitions, which is measured to be before vanilla builds its items, so a block item finds its block; an earlier attempt to move block registration into the item pass was reverted, along with the switch to a ModAPI owned id range
- `src-test/BlockTest.cpp` overrides virtuals on both sides and asserts the effect: the block item's engine fields (max stack size 16, foil), an engine virtual (`isMusicDisk`), the item -> block link (`mBlockType`), and a block hook (`canProvideSupport`). A plain `ICustomItem`'s overrides are only *recorded*, with the reason: its engine object is replaced by the engine in a later pass (measured: the registry holds a different pointer than the one ModAPI built), so its fields read the defaults - a pre-existing ModAPI issue, out of scope here. Note also that this build has no RTTI data, so `dynamic_cast` aborts ("no RTTI data") and a `static_cast` on a pointer you own is the way to go

- Block items now send a client definition of their own: `NetworkTagBuilder` gained a `buildClientComponents(ICustomBlockItem const&)` overload and `ICustomBlockItem` gained `getIcon`, `getDisplayName` and a `buildNetworkTag` override. Without it the creative entry for a block item had no texture (the block itself rendered, so the packs were delivered - the item definition simply had no icon) and the name stayed an untranslated key. Measured after the change: the item tag carries the texture name (`blockItem.tagHasIcon`). The test also names its block item exactly like its block, as vanilla does (`minecraft:stone` is both), which the engine's block -> item resolution for pick block relies on, and its resource pack now ships `texts/en_US.lang` / `texts/zh_CN.lang`

### Fixed

- The vanilla `minecraft:stick` losing its icon on a client (`Missing icon for data-driven item 'minecraft:stick'`) was caused by this repository's own `[REGISTRY]` suite: it exercised the vanilla item setters (`setIcon`, `setDisplayName`, `addTag`, `setFireResistant`, `setRepairItem`, `getAndModifyVanillaNetworkTagInfo`) on `minecraft:stick`, which fills `mModifiedVanillaItems` and makes `VanillaItemDefinitionSendHook` replace that item's `components` in the registry packet. The suite now modifies the item it registers itself (`modapi_test:test_item`) and only reads the vanilla one. Measured: with both mods loaded the stick's definition is no longer touched, and all four suites stay green. It was *not* the block item, the runtime packs, `ItemVersion::DataDriven`, item id collisions or the creative pass - each of those was ruled out by an A/B, several of which were invalid until the manifest explained why: `ModAPI` is `"passive": true` and `test` is its only dependent, so renaming `test` away also unloaded ModAPI (LeviLamina reported "loaded 0 mods"), which is what made "disable the test mod" look like a fix
- Follow-up to that, and the reason it was so damaging: `VanillaItemDefinitionSendHook` used to *replace* an item's whole `components` node with the tag recorded by the setters, and that tag is built from `Item::buildNetworkTag()` - for a data driven vanilla item nearly empty (measured: `minecraft:stick` came out as 37 chars holding only `minecraft:hand_equipped`). So modifying such an item dropped everything it had. The hook now *merges* the recorded changes into what the engine already publishes for the item (top level, and inside `item_properties`), which keeps the data the mod did not touch - the icon of a modified item, for instance

- Re-registering an item name left the replaced item in `ItemRegistry::mItemRegistry`: the new item was appended while the old one stayed in the vector, so every pass that walks it (the client definition build, and this API's own `forEachItemInRegistry`) saw two items answering to one name. The leftover carried none of the values its mod had applied (`max stack size` 64 instead of 16), and the object handed back by the registration differed from the one the registry exposed. Registration now drops the replaced entry, and a custom item's `_init` values survive - verified server side: `plain item fields: stack = 16, glint = true`, and the registry object and the registration product are the same pointer

- `/give <custom item>` silently produced nothing: the enum entry was written as `CommandItem{id, true, false}`, but the type is a union of `{ short mVersion; bool mOverrideAux; int mId; }` over a `uint64`. The id therefore landed in `mVersion` (truncated) and `mId` was left 0. Reading the engine's own `CommandItem::createInstance` settled the rest: it only treats `mId` as the item's own id when `mVersion` is **non-zero** - with 0 it converts the value as a *legacy* id through a different table, which is why an entry for `minecraft:stick` resolved to `minecraft:oak_sign` while our item resolved to nothing. The entry is now `{1, true, id}`. The registry suite builds that value and asserts `createInstance` returns the item (`give.*`)
- Note for whoever tests this next: the item must be reached by a **real** player - a virtual one gives `No targets matched selector`; and an item whose identifier contains a dot (`modapi_test:item.test_block`) is rejected by the command parser outright, so a block item registered under that name cannot be given at all

- The definition packet a client receives was never filled in for custom items. `ModAPI` hooked
  `ItemRegistryPacket::write`, but the old build shows the packet is serialized through
  `writeWithSerializationMode` reaching `serialize<ItemRegistryPacketPayload>::write` directly - `write` is a
  different virtual, so the hook never ran. It is now on the payload constructor, which is where `mItems` is
  produced and which every sending path goes through. Reading `components` out of an entry that has none also
  threw `bad variant access` and aborted the whole merge, so the node is only read after a check
- `ICustomItem` stopped setting `mItemParseVersion = ItemVersion::DataDriven`: it was commented out during an
  experiment and left that way. That flag is what gives an item the definition machinery a client needs, and
  custom items came up blank from then on (`Item <name> requires either an icon atlas or icon texture`)
- Re-registering an item name left the replaced item in `ItemRegistry::mItemRegistry`, and
  `CreativeItemRegistry` appended a creative entry per registration, so items showed up twice in a client's
  inventory and the leftover reported default fields. Registration now drops the replaced entry, and
  `registerCreativeItem` replaces the entry for an item instead of appending another
- `/give <custom item>` silently did nothing: the item enum entry was written as `CommandItem{id, true, false}`,
  but the type is a union of `{ short mVersion; bool mOverrideAux; int mId; }`. `CommandItem::createInstance`
  only reads `mId` as the item's own id when `mVersion` is non-zero - with 0 it converts the value as a *legacy*
  id through a different table
- A block item's id has to be the block's id folded into a short (`id <= 0xFF ? id : 255 - id`), which is what
  `ItemRegistryRef::registerBlockItem<BlockItem>` hands to `BlockItem`'s constructor. Assigning an id afterwards
  left the item's own state disagreeing with `mId`, which showed up as a creative entry nothing could be done
  with
- A block whose type a mod registers in C++ is not listed by the engine at all: the engine creates items for the
  blocks it parses out of documents, so that item is queued for the creative pass instead. The queue cannot
  duplicate anything now that `registerCreativeItem` replaces

- A block item drawn as the block itself (rather than as a flat picture) needs the `minecraft:block_placer`
  component in its definition, with the block it stands for. Two details decide whether a client acts on it, and
  both come out of `PlanterItemComponent::buildNetworkTag` in the old build, which writes `canUseBlockAsIcon` from
  the component's byte +64 and `replaceBlockItem` from +65: the keys are **camelCase** in the network form (a
  *document* uses `replace_block_item`, bound by `bindType`), and `canUseBlockAsIcon` has no document key at all -
  the definition sent to a client is the only place it can be set. A `minecraft:icon` must be left out at the same
  time: `ComponentItem::getIconInfo` reads the icon component first and only falls back to the item's block when
  there is none, so writing an icon pins the item to a texture

- A block registered from C++ now reaches clients without any document. The engine builds the list a client is
  told about (`StartGamePacketPayload::mBlockProperties`) out of the block definition group, and a block whose type
  ModAPI registered itself is not in that group - so the packet's write path adds the entry it is missing. The entry
  is field for field the one the engine writes itself (read out of a live one as SNBT), which is why it works: the
  same `components`/`minecraft:material_instances`, `menu_category`, `molangVersion` and `vanilla_block_data`, with
  the block's own id. `ServerBlockProperty`'s default constructor is declared in the header but not exported, so its
  definition is written here.
- The document path is gone with it: `registerBlockFromMemoryJson`, `registerBlockFromJsonFile`, `deferBlockDocument`
  and the runtime pack they installed were removed, along with the suppression of the engine's block type and the
  bookkeeping that went with it. A block is a `::BlockType` subclass registered under an identifier, and the texture
  a client draws it with is named with `BlockRegistry::setBlockTexture` - the texture file and the resource pack
  mappings (`blocks.json`, `terrain_texture.json`) are the mod's own, exactly as they are for an addon block.
- `ICustomBlock` is gone as well: it only forwarded a constructor to `::BlockType`, so a mod derives from
  `::BlockType` directly. `ICustomFeature` moved to `include/modapi/worldgen/base/` and
  `src/modapi/worldgen/base/`, which is where every other registry keeps its interfaces.

- The definition-level part of a block entry comes out of the registration's own `BlockDescription` instead of
  defaults: `menu_category.category` through the engine's exported
  `SharedTypes::v1_21_110::ItemCategory::stringFromCreativeItemCategory`, `group` and `is_hidden_in_commands` from
  `BlockMenuCategory`, and `vanilla_block_data.block_id` / `material` from `VanillaBlockData` (the material name
  through `BlockEnum::MaterialTypeToString`). `BlockPermutationDescription`s are published as `permutations`, each
  with its `condition` (`ExpressionNode::getExpressionString`), its components and its tags - the same three nodes the
  engine writes.
- A block is registered with what to tell a client about it, in one call and in the engine's own types:
  `registerBlock<MyBlock>("mymod:my_block", { .mArguments = {...}, .mProperty = {...} })`, with `BlockProperty` made
  of `BlockComponentGroupDescription` / `BlockDescription` / `BlockPermutationDescription`. The same property drives
  both sides: its components are injected into the running block type through `initializeComponentFromCode` - the
  route vanilla uses for its own blocks - and serialised for a client through `isNetworkComponent` + `getName` +
  `buildNetworkTag`, which are the calls `generateServerBlockProperties` makes. The entry produced this way matches
  the engine's own field for field (measured against the entry the engine writes for an addon block: identical apart
  from each block's own texture name and id). No `setBlockTexture`/`setBlockProperty`: a property is handed over with
  the registration.
- The engine declares default constructors and copy operations for those definition types and does not export them,
  so `include/modapi/block/base/EngineDefaults.h` supplies the definitions inline - default constructors only, and
  copy/assignment only where the linker proved them missing. A type holding a `CompoundTag`
  (`ServerBlockProperty`) keeps the engine's exported copy: a memberwise `= default` there copies the tag shallowly
  and two objects then free the same storage, which crashed the server at `StartGamePacket` time.

### Changed

- `RuntimePack` gets a fresh uuid on every construction instead of a name derived one (and logs it): a client that already has a pack with the same uuid does not download it again, so a pack rebuilt on each server start has to offer an identity no client has seen. Packs also have to be installed while mods load - the stack clients are offered is composed when a level starts, so a pack installed later (from a command, for instance) never reaches them; the test installs its packs there now

- The test ships a real 16x16 PNG for its block texture (embedded, generated with Pillow, also kept at `src-test/assets/modapi_test_block.png`) instead of a placeholder file, and asserts the shipped file carries the PNG signature: a placeholder leaves a client with nothing to draw, which was a real defect spotted in review

### Added

- `modapi::item::ICustomBlockItem` (`include/modapi/item/base/ICustomBlockItem.h`): the item that places a custom block. It derives from the engine's `::BlockItem` and is registered through `ItemRegistry` like any other item (`isEntry` already accepted `std::derived_from<Entry, ::Item>`, so nothing had to be loosened), with an `initCustomItem(ICustomBlockItem&)` overload next to the existing `ICustomItem`/`ICustomArmorItem` ones. Measured: `::BlockItem` does *not* link itself to its block (`getBlockTypeForRendering` is not even overridden by it) - the link is `::Item::mBlockType`, so the class sets it in `_init`; before that an item built for a custom block reported `minecraft:air`, afterwards it reports the block. Registration timing needed no change either: block types are registered right after the level reads its definitions, which is measured to be before vanilla builds its items, so a block item finds its block; an earlier attempt to move block registration into the item pass was reverted, along with the switch to a ModAPI owned id range
- `src-test/BlockTest.cpp` overrides virtuals on both sides and asserts the effect: the block item's engine fields (max stack size 16, foil), an engine virtual (`isMusicDisk`), the item -> block link (`mBlockType`), and a block hook (`canProvideSupport`). A plain `ICustomItem`'s overrides are only *recorded*, with the reason: its engine object is replaced by the engine in a later pass (measured: the registry holds a different pointer than the one ModAPI built), so its fields read the defaults - a pre-existing ModAPI issue, out of scope here. Note also that this build has no RTTI data, so `dynamic_cast` aborts ("no RTTI data") and a `static_cast` on a pointer you own is the way to go

- Block items now send a client definition of their own: `NetworkTagBuilder` gained a `buildClientComponents(ICustomBlockItem const&)` overload and `ICustomBlockItem` gained `getIcon`, `getDisplayName` and a `buildNetworkTag` override. Without it the creative entry for a block item had no texture (the block itself rendered, so the packs were delivered - the item definition simply had no icon) and the name stayed an untranslated key. Measured after the change: the item tag carries the texture name (`blockItem.tagHasIcon`). The test also names its block item exactly like its block, as vanilla does (`minecraft:stone` is both), which the engine's block -> item resolution for pick block relies on, and its resource pack now ships `texts/en_US.lang` / `texts/zh_CN.lang`

### Fixed

- The vanilla `minecraft:stick` losing its icon on a client (`Missing icon for data-driven item 'minecraft:stick'`) was caused by this repository's own `[REGISTRY]` suite: it exercised the vanilla item setters (`setIcon`, `setDisplayName`, `addTag`, `setFireResistant`, `setRepairItem`, `getAndModifyVanillaNetworkTagInfo`) on `minecraft:stick`, which fills `mModifiedVanillaItems` and makes `VanillaItemDefinitionSendHook` replace that item's `components` in the registry packet. The suite now modifies the item it registers itself (`modapi_test:test_item`) and only reads the vanilla one. Measured: with both mods loaded the stick's definition is no longer touched, and all four suites stay green. It was *not* the block item, the runtime packs, `ItemVersion::DataDriven`, item id collisions or the creative pass - each of those was ruled out by an A/B, several of which were invalid until the manifest explained why: `ModAPI` is `"passive": true` and `test` is its only dependent, so renaming `test` away also unloaded ModAPI (LeviLamina reported "loaded 0 mods"), which is what made "disable the test mod" look like a fix
- Follow-up to that, and the reason it was so damaging: `VanillaItemDefinitionSendHook` used to *replace* an item's whole `components` node with the tag recorded by the setters, and that tag is built from `Item::buildNetworkTag()` - for a data driven vanilla item nearly empty (measured: `minecraft:stick` came out as 37 chars holding only `minecraft:hand_equipped`). So modifying such an item dropped everything it had. The hook now *merges* the recorded changes into what the engine already publishes for the item (top level, and inside `item_properties`), which keeps the data the mod did not touch - the icon of a modified item, for instance

- Re-registering an item name left the replaced item in `ItemRegistry::mItemRegistry`: the new item was appended while the old one stayed in the vector, so every pass that walks it (the client definition build, and this API's own `forEachItemInRegistry`) saw two items answering to one name. The leftover carried none of the values its mod had applied (`max stack size` 64 instead of 16), and the object handed back by the registration differed from the one the registry exposed. Registration now drops the replaced entry, and a custom item's `_init` values survive - verified server side: `plain item fields: stack = 16, glint = true`, and the registry object and the registration product are the same pointer

- `/give <custom item>` silently produced nothing: the enum entry was written as `CommandItem{id, true, false}`, but the type is a union of `{ short mVersion; bool mOverrideAux; int mId; }` over a `uint64`. The id therefore landed in `mVersion` (truncated) and `mId` was left 0. Reading the engine's own `CommandItem::createInstance` settled the rest: it only treats `mId` as the item's own id when `mVersion` is **non-zero** - with 0 it converts the value as a *legacy* id through a different table, which is why an entry for `minecraft:stick` resolved to `minecraft:oak_sign` while our item resolved to nothing. The entry is now `{1, true, id}`. The registry suite builds that value and asserts `createInstance` returns the item (`give.*`)
- Note for whoever tests this next: the item must be reached by a **real** player - a virtual one gives `No targets matched selector`; and an item whose identifier contains a dot (`modapi_test:item.test_block`) is rejected by the command parser outright, so a block item registered under that name cannot be given at all

- The definition packet a client receives was never filled in for custom items. `ModAPI` hooked
  `ItemRegistryPacket::write`, but the old build shows the packet is serialized through
  `writeWithSerializationMode` reaching `serialize<ItemRegistryPacketPayload>::write` directly - `write` is a
  different virtual, so the hook never ran. It is now on the payload constructor, which is where `mItems` is
  produced and which every sending path goes through. Reading `components` out of an entry that has none also
  threw `bad variant access` and aborted the whole merge, so the node is only read after a check
- `ICustomItem` stopped setting `mItemParseVersion = ItemVersion::DataDriven`: it was commented out during an
  experiment and left that way. That flag is what gives an item the definition machinery a client needs, and
  custom items came up blank from then on (`Item <name> requires either an icon atlas or icon texture`)
- Re-registering an item name left the replaced item in `ItemRegistry::mItemRegistry`, and
  `CreativeItemRegistry` appended a creative entry per registration, so items showed up twice in a client's
  inventory and the leftover reported default fields. Registration now drops the replaced entry, and
  `registerCreativeItem` replaces the entry for an item instead of appending another
- `/give <custom item>` silently did nothing: the item enum entry was written as `CommandItem{id, true, false}`,
  but the type is a union of `{ short mVersion; bool mOverrideAux; int mId; }`. `CommandItem::createInstance`
  only reads `mId` as the item's own id when `mVersion` is non-zero - with 0 it converts the value as a *legacy*
  id through a different table
- A block item's id has to be the block's id folded into a short (`id <= 0xFF ? id : 255 - id`), which is what
  `ItemRegistryRef::registerBlockItem<BlockItem>` hands to `BlockItem`'s constructor. Assigning an id afterwards
  left the item's own state disagreeing with `mId`, which showed up as a creative entry nothing could be done
  with
- A block whose type a mod registers in C++ is not listed by the engine at all: the engine creates items for the
  blocks it parses out of documents, so that item is queued for the creative pass instead. The queue cannot
  duplicate anything now that `registerCreativeItem` replaces

- A block item drawn as the block itself (rather than as a flat picture) needs the `minecraft:block_placer`
  component in its definition, with the block it stands for. Two details decide whether a client acts on it, and
  both come out of `PlanterItemComponent::buildNetworkTag` in the old build, which writes `canUseBlockAsIcon` from
  the component's byte +64 and `replaceBlockItem` from +65: the keys are **camelCase** in the network form (a
  *document* uses `replace_block_item`, bound by `bindType`), and `canUseBlockAsIcon` has no document key at all -
  the definition sent to a client is the only place it can be set. A `minecraft:icon` must be left out at the same
  time: `ComponentItem::getIconInfo` reads the icon component first and only falls back to the item's block when
  there is none, so writing an icon pins the item to a texture

- A block registered from C++ now reaches clients without any document. The engine builds the list a client is
  told about (`StartGamePacketPayload::mBlockProperties`) out of the block definition group, and a block whose type
  ModAPI registered itself is not in that group - so the packet's write path adds the entry it is missing. The entry
  is field for field the one the engine writes itself (read out of a live one as SNBT), which is why it works: the
  same `components`/`minecraft:material_instances`, `menu_category`, `molangVersion` and `vanilla_block_data`, with
  the block's own id. `ServerBlockProperty`'s default constructor is declared in the header but not exported, so its
  definition is written here.
- The document path is gone with it: `registerBlockFromMemoryJson`, `registerBlockFromJsonFile`, `deferBlockDocument`
  and the runtime pack they installed were removed, along with the suppression of the engine's block type and the
  bookkeeping that went with it. A block is a `::BlockType` subclass registered under an identifier, and the texture
  a client draws it with is named with `BlockRegistry::setBlockTexture` - the texture file and the resource pack
  mappings (`blocks.json`, `terrain_texture.json`) are the mod's own, exactly as they are for an addon block.
- `ICustomBlock` is gone as well: it only forwarded a constructor to `::BlockType`, so a mod derives from
  `::BlockType` directly. `ICustomFeature` moved to `include/modapi/worldgen/base/` and
  `src/modapi/worldgen/base/`, which is where every other registry keeps its interfaces.

- The definition-level part of a block entry comes out of the registration's own `BlockDescription` instead of
  defaults: `menu_category.category` through the engine's exported
  `SharedTypes::v1_21_110::ItemCategory::stringFromCreativeItemCategory`, `group` and `is_hidden_in_commands` from
  `BlockMenuCategory`, and `vanilla_block_data.block_id` / `material` from `VanillaBlockData` (the material name
  through `BlockEnum::MaterialTypeToString`). `BlockPermutationDescription`s are published as `permutations`, each
  with its `condition` (`ExpressionNode::getExpressionString`), its components and its tags - the same three nodes the
  engine writes.
- A block is registered with what to tell a client about it, in one call and in the engine's own types:
  `registerBlock<MyBlock>("mymod:my_block", { .mArguments = {...}, .mProperty = {...} })`, with `BlockProperty` made
  of `BlockComponentGroupDescription` / `BlockDescription` / `BlockPermutationDescription`. The same property drives
  both sides: its components are injected into the running block type through `initializeComponentFromCode` - the
  route vanilla uses for its own blocks - and serialised for a client through `isNetworkComponent` + `getName` +
  `buildNetworkTag`, which are the calls `generateServerBlockProperties` makes. The entry produced this way matches
  the engine's own field for field (measured against the entry the engine writes for an addon block: identical apart
  from each block's own texture name and id). No `setBlockTexture`/`setBlockProperty`: a property is handed over with
  the registration.
- The engine declares default constructors and copy operations for those definition types and does not export them,
  so `include/modapi/block/base/EngineDefaults.h` supplies the definitions inline - default constructors only, and
  copy/assignment only where the linker proved them missing. A type holding a `CompoundTag`
  (`ServerBlockProperty`) keeps the engine's exported copy: a memberwise `= default` there copies the tag shallowly
  and two objects then free the same storage, which crashed the server at `StartGamePacket` time.

### Changed

- Adapted LeviLamina 1.7.x and BDS 1.21.120 @zimuya4153

## [0.2.1] - 2025-10-23

### Fixed

- Fixed the Item link issue temporarily @zimuya4153

## [0.2.0] - 2025-10-21

- The test ships a real 16x16 PNG for its block texture (embedded, generated with Pillow, also kept at `src-test/assets/modapi_test_block.png`) instead of a placeholder file, and asserts the shipped file carries the PNG signature: a placeholder leaves a client with nothing to draw, which was a real defect spotted in review

### Added

- `modapi::item::ICustomBlockItem` (`include/modapi/item/base/ICustomBlockItem.h`): the item that places a custom block. It derives from the engine's `::BlockItem` and is registered through `ItemRegistry` like any other item (`isEntry` already accepted `std::derived_from<Entry, ::Item>`, so nothing had to be loosened), with an `initCustomItem(ICustomBlockItem&)` overload next to the existing `ICustomItem`/`ICustomArmorItem` ones. Measured: `::BlockItem` does *not* link itself to its block (`getBlockTypeForRendering` is not even overridden by it) - the link is `::Item::mBlockType`, so the class sets it in `_init`; before that an item built for a custom block reported `minecraft:air`, afterwards it reports the block. Registration timing needed no change either: block types are registered right after the level reads its definitions, which is measured to be before vanilla builds its items, so a block item finds its block; an earlier attempt to move block registration into the item pass was reverted, along with the switch to a ModAPI owned id range
- `src-test/BlockTest.cpp` overrides virtuals on both sides and asserts the effect: the block item's engine fields (max stack size 16, foil), an engine virtual (`isMusicDisk`), the item -> block link (`mBlockType`), and a block hook (`canProvideSupport`). A plain `ICustomItem`'s overrides are only *recorded*, with the reason: its engine object is replaced by the engine in a later pass (measured: the registry holds a different pointer than the one ModAPI built), so its fields read the defaults - a pre-existing ModAPI issue, out of scope here. Note also that this build has no RTTI data, so `dynamic_cast` aborts ("no RTTI data") and a `static_cast` on a pointer you own is the way to go

- Block items now send a client definition of their own: `NetworkTagBuilder` gained a `buildClientComponents(ICustomBlockItem const&)` overload and `ICustomBlockItem` gained `getIcon`, `getDisplayName` and a `buildNetworkTag` override. Without it the creative entry for a block item had no texture (the block itself rendered, so the packs were delivered - the item definition simply had no icon) and the name stayed an untranslated key. Measured after the change: the item tag carries the texture name (`blockItem.tagHasIcon`). The test also names its block item exactly like its block, as vanilla does (`minecraft:stone` is both), which the engine's block -> item resolution for pick block relies on, and its resource pack now ships `texts/en_US.lang` / `texts/zh_CN.lang`

### Fixed

- The vanilla `minecraft:stick` losing its icon on a client (`Missing icon for data-driven item 'minecraft:stick'`) was caused by this repository's own `[REGISTRY]` suite: it exercised the vanilla item setters (`setIcon`, `setDisplayName`, `addTag`, `setFireResistant`, `setRepairItem`, `getAndModifyVanillaNetworkTagInfo`) on `minecraft:stick`, which fills `mModifiedVanillaItems` and makes `VanillaItemDefinitionSendHook` replace that item's `components` in the registry packet. The suite now modifies the item it registers itself (`modapi_test:test_item`) and only reads the vanilla one. Measured: with both mods loaded the stick's definition is no longer touched, and all four suites stay green. It was *not* the block item, the runtime packs, `ItemVersion::DataDriven`, item id collisions or the creative pass - each of those was ruled out by an A/B, several of which were invalid until the manifest explained why: `ModAPI` is `"passive": true` and `test` is its only dependent, so renaming `test` away also unloaded ModAPI (LeviLamina reported "loaded 0 mods"), which is what made "disable the test mod" look like a fix
- Follow-up to that, and the reason it was so damaging: `VanillaItemDefinitionSendHook` used to *replace* an item's whole `components` node with the tag recorded by the setters, and that tag is built from `Item::buildNetworkTag()` - for a data driven vanilla item nearly empty (measured: `minecraft:stick` came out as 37 chars holding only `minecraft:hand_equipped`). So modifying such an item dropped everything it had. The hook now *merges* the recorded changes into what the engine already publishes for the item (top level, and inside `item_properties`), which keeps the data the mod did not touch - the icon of a modified item, for instance

- Re-registering an item name left the replaced item in `ItemRegistry::mItemRegistry`: the new item was appended while the old one stayed in the vector, so every pass that walks it (the client definition build, and this API's own `forEachItemInRegistry`) saw two items answering to one name. The leftover carried none of the values its mod had applied (`max stack size` 64 instead of 16), and the object handed back by the registration differed from the one the registry exposed. Registration now drops the replaced entry, and a custom item's `_init` values survive - verified server side: `plain item fields: stack = 16, glint = true`, and the registry object and the registration product are the same pointer

- `/give <custom item>` silently produced nothing: the enum entry was written as `CommandItem{id, true, false}`, but the type is a union of `{ short mVersion; bool mOverrideAux; int mId; }` over a `uint64`. The id therefore landed in `mVersion` (truncated) and `mId` was left 0. Reading the engine's own `CommandItem::createInstance` settled the rest: it only treats `mId` as the item's own id when `mVersion` is **non-zero** - with 0 it converts the value as a *legacy* id through a different table, which is why an entry for `minecraft:stick` resolved to `minecraft:oak_sign` while our item resolved to nothing. The entry is now `{1, true, id}`. The registry suite builds that value and asserts `createInstance` returns the item (`give.*`)
- Note for whoever tests this next: the item must be reached by a **real** player - a virtual one gives `No targets matched selector`; and an item whose identifier contains a dot (`modapi_test:item.test_block`) is rejected by the command parser outright, so a block item registered under that name cannot be given at all

- The definition packet a client receives was never filled in for custom items. `ModAPI` hooked
  `ItemRegistryPacket::write`, but the old build shows the packet is serialized through
  `writeWithSerializationMode` reaching `serialize<ItemRegistryPacketPayload>::write` directly - `write` is a
  different virtual, so the hook never ran. It is now on the payload constructor, which is where `mItems` is
  produced and which every sending path goes through. Reading `components` out of an entry that has none also
  threw `bad variant access` and aborted the whole merge, so the node is only read after a check
- `ICustomItem` stopped setting `mItemParseVersion = ItemVersion::DataDriven`: it was commented out during an
  experiment and left that way. That flag is what gives an item the definition machinery a client needs, and
  custom items came up blank from then on (`Item <name> requires either an icon atlas or icon texture`)
- Re-registering an item name left the replaced item in `ItemRegistry::mItemRegistry`, and
  `CreativeItemRegistry` appended a creative entry per registration, so items showed up twice in a client's
  inventory and the leftover reported default fields. Registration now drops the replaced entry, and
  `registerCreativeItem` replaces the entry for an item instead of appending another
- `/give <custom item>` silently did nothing: the item enum entry was written as `CommandItem{id, true, false}`,
  but the type is a union of `{ short mVersion; bool mOverrideAux; int mId; }`. `CommandItem::createInstance`
  only reads `mId` as the item's own id when `mVersion` is non-zero - with 0 it converts the value as a *legacy*
  id through a different table
- A block item's id has to be the block's id folded into a short (`id <= 0xFF ? id : 255 - id`), which is what
  `ItemRegistryRef::registerBlockItem<BlockItem>` hands to `BlockItem`'s constructor. Assigning an id afterwards
  left the item's own state disagreeing with `mId`, which showed up as a creative entry nothing could be done
  with
- A block whose type a mod registers in C++ is not listed by the engine at all: the engine creates items for the
  blocks it parses out of documents, so that item is queued for the creative pass instead. The queue cannot
  duplicate anything now that `registerCreativeItem` replaces

- A block item drawn as the block itself (rather than as a flat picture) needs the `minecraft:block_placer`
  component in its definition, with the block it stands for. Two details decide whether a client acts on it, and
  both come out of `PlanterItemComponent::buildNetworkTag` in the old build, which writes `canUseBlockAsIcon` from
  the component's byte +64 and `replaceBlockItem` from +65: the keys are **camelCase** in the network form (a
  *document* uses `replace_block_item`, bound by `bindType`), and `canUseBlockAsIcon` has no document key at all -
  the definition sent to a client is the only place it can be set. A `minecraft:icon` must be left out at the same
  time: `ComponentItem::getIconInfo` reads the icon component first and only falls back to the item's block when
  there is none, so writing an icon pins the item to a texture

- A block registered from C++ now reaches clients without any document. The engine builds the list a client is
  told about (`StartGamePacketPayload::mBlockProperties`) out of the block definition group, and a block whose type
  ModAPI registered itself is not in that group - so the packet's write path adds the entry it is missing. The entry
  is field for field the one the engine writes itself (read out of a live one as SNBT), which is why it works: the
  same `components`/`minecraft:material_instances`, `menu_category`, `molangVersion` and `vanilla_block_data`, with
  the block's own id. `ServerBlockProperty`'s default constructor is declared in the header but not exported, so its
  definition is written here.
- The document path is gone with it: `registerBlockFromMemoryJson`, `registerBlockFromJsonFile`, `deferBlockDocument`
  and the runtime pack they installed were removed, along with the suppression of the engine's block type and the
  bookkeeping that went with it. A block is a `::BlockType` subclass registered under an identifier, and the texture
  a client draws it with is named with `BlockRegistry::setBlockTexture` - the texture file and the resource pack
  mappings (`blocks.json`, `terrain_texture.json`) are the mod's own, exactly as they are for an addon block.
- `ICustomBlock` is gone as well: it only forwarded a constructor to `::BlockType`, so a mod derives from
  `::BlockType` directly. `ICustomFeature` moved to `include/modapi/worldgen/base/` and
  `src/modapi/worldgen/base/`, which is where every other registry keeps its interfaces.

- The definition-level part of a block entry comes out of the registration's own `BlockDescription` instead of
  defaults: `menu_category.category` through the engine's exported
  `SharedTypes::v1_21_110::ItemCategory::stringFromCreativeItemCategory`, `group` and `is_hidden_in_commands` from
  `BlockMenuCategory`, and `vanilla_block_data.block_id` / `material` from `VanillaBlockData` (the material name
  through `BlockEnum::MaterialTypeToString`). `BlockPermutationDescription`s are published as `permutations`, each
  with its `condition` (`ExpressionNode::getExpressionString`), its components and its tags - the same three nodes the
  engine writes.
- A block is registered with what to tell a client about it, in one call and in the engine's own types:
  `registerBlock<MyBlock>("mymod:my_block", { .mArguments = {...}, .mProperty = {...} })`, with `BlockProperty` made
  of `BlockComponentGroupDescription` / `BlockDescription` / `BlockPermutationDescription`. The same property drives
  both sides: its components are injected into the running block type through `initializeComponentFromCode` - the
  route vanilla uses for its own blocks - and serialised for a client through `isNetworkComponent` + `getName` +
  `buildNetworkTag`, which are the calls `generateServerBlockProperties` makes. The entry produced this way matches
  the engine's own field for field (measured against the entry the engine writes for an addon block: identical apart
  from each block's own texture name and id). No `setBlockTexture`/`setBlockProperty`: a property is handed over with
  the registration.
- The engine declares default constructors and copy operations for those definition types and does not export them,
  so `include/modapi/block/base/EngineDefaults.h` supplies the definitions inline - default constructors only, and
  copy/assignment only where the linker proved them missing. A type holding a `CompoundTag`
  (`ServerBlockProperty`) keeps the engine's exported copy: a memberwise `= default` there copies the tag shallowly
  and two objects then free the same storage, which crashed the server at `StartGamePacket` time.

### Changed

- `RuntimePack` gets a fresh uuid on every construction instead of a name derived one (and logs it): a client that already has a pack with the same uuid does not download it again, so a pack rebuilt on each server start has to offer an identity no client has seen. Packs also have to be installed while mods load - the stack clients are offered is composed when a level starts, so a pack installed later (from a command, for instance) never reaches them; the test installs its packs there now

- The test ships a real 16x16 PNG for its block texture (embedded, generated with Pillow, also kept at `src-test/assets/modapi_test_block.png`) instead of a placeholder file, and asserts the shipped file carries the PNG signature: a placeholder leaves a client with nothing to draw, which was a real defect spotted in review

### Added

- `modapi::item::ICustomBlockItem` (`include/modapi/item/base/ICustomBlockItem.h`): the item that places a custom block. It derives from the engine's `::BlockItem` and is registered through `ItemRegistry` like any other item (`isEntry` already accepted `std::derived_from<Entry, ::Item>`, so nothing had to be loosened), with an `initCustomItem(ICustomBlockItem&)` overload next to the existing `ICustomItem`/`ICustomArmorItem` ones. Measured: `::BlockItem` does *not* link itself to its block (`getBlockTypeForRendering` is not even overridden by it) - the link is `::Item::mBlockType`, so the class sets it in `_init`; before that an item built for a custom block reported `minecraft:air`, afterwards it reports the block. Registration timing needed no change either: block types are registered right after the level reads its definitions, which is measured to be before vanilla builds its items, so a block item finds its block; an earlier attempt to move block registration into the item pass was reverted, along with the switch to a ModAPI owned id range
- `src-test/BlockTest.cpp` overrides virtuals on both sides and asserts the effect: the block item's engine fields (max stack size 16, foil), an engine virtual (`isMusicDisk`), the item -> block link (`mBlockType`), and a block hook (`canProvideSupport`). A plain `ICustomItem`'s overrides are only *recorded*, with the reason: its engine object is replaced by the engine in a later pass (measured: the registry holds a different pointer than the one ModAPI built), so its fields read the defaults - a pre-existing ModAPI issue, out of scope here. Note also that this build has no RTTI data, so `dynamic_cast` aborts ("no RTTI data") and a `static_cast` on a pointer you own is the way to go

- Block items now send a client definition of their own: `NetworkTagBuilder` gained a `buildClientComponents(ICustomBlockItem const&)` overload and `ICustomBlockItem` gained `getIcon`, `getDisplayName` and a `buildNetworkTag` override. Without it the creative entry for a block item had no texture (the block itself rendered, so the packs were delivered - the item definition simply had no icon) and the name stayed an untranslated key. Measured after the change: the item tag carries the texture name (`blockItem.tagHasIcon`). The test also names its block item exactly like its block, as vanilla does (`minecraft:stone` is both), which the engine's block -> item resolution for pick block relies on, and its resource pack now ships `texts/en_US.lang` / `texts/zh_CN.lang`

### Fixed

- The vanilla `minecraft:stick` losing its icon on a client (`Missing icon for data-driven item 'minecraft:stick'`) was caused by this repository's own `[REGISTRY]` suite: it exercised the vanilla item setters (`setIcon`, `setDisplayName`, `addTag`, `setFireResistant`, `setRepairItem`, `getAndModifyVanillaNetworkTagInfo`) on `minecraft:stick`, which fills `mModifiedVanillaItems` and makes `VanillaItemDefinitionSendHook` replace that item's `components` in the registry packet. The suite now modifies the item it registers itself (`modapi_test:test_item`) and only reads the vanilla one. Measured: with both mods loaded the stick's definition is no longer touched, and all four suites stay green. It was *not* the block item, the runtime packs, `ItemVersion::DataDriven`, item id collisions or the creative pass - each of those was ruled out by an A/B, several of which were invalid until the manifest explained why: `ModAPI` is `"passive": true` and `test` is its only dependent, so renaming `test` away also unloaded ModAPI (LeviLamina reported "loaded 0 mods"), which is what made "disable the test mod" look like a fix
- Follow-up to that, and the reason it was so damaging: `VanillaItemDefinitionSendHook` used to *replace* an item's whole `components` node with the tag recorded by the setters, and that tag is built from `Item::buildNetworkTag()` - for a data driven vanilla item nearly empty (measured: `minecraft:stick` came out as 37 chars holding only `minecraft:hand_equipped`). So modifying such an item dropped everything it had. The hook now *merges* the recorded changes into what the engine already publishes for the item (top level, and inside `item_properties`), which keeps the data the mod did not touch - the icon of a modified item, for instance

- Re-registering an item name left the replaced item in `ItemRegistry::mItemRegistry`: the new item was appended while the old one stayed in the vector, so every pass that walks it (the client definition build, and this API's own `forEachItemInRegistry`) saw two items answering to one name. The leftover carried none of the values its mod had applied (`max stack size` 64 instead of 16), and the object handed back by the registration differed from the one the registry exposed. Registration now drops the replaced entry, and a custom item's `_init` values survive - verified server side: `plain item fields: stack = 16, glint = true`, and the registry object and the registration product are the same pointer

- `/give <custom item>` silently produced nothing: the enum entry was written as `CommandItem{id, true, false}`, but the type is a union of `{ short mVersion; bool mOverrideAux; int mId; }` over a `uint64`. The id therefore landed in `mVersion` (truncated) and `mId` was left 0. Reading the engine's own `CommandItem::createInstance` settled the rest: it only treats `mId` as the item's own id when `mVersion` is **non-zero** - with 0 it converts the value as a *legacy* id through a different table, which is why an entry for `minecraft:stick` resolved to `minecraft:oak_sign` while our item resolved to nothing. The entry is now `{1, true, id}`. The registry suite builds that value and asserts `createInstance` returns the item (`give.*`)
- Note for whoever tests this next: the item must be reached by a **real** player - a virtual one gives `No targets matched selector`; and an item whose identifier contains a dot (`modapi_test:item.test_block`) is rejected by the command parser outright, so a block item registered under that name cannot be given at all

- The definition packet a client receives was never filled in for custom items. `ModAPI` hooked
  `ItemRegistryPacket::write`, but the old build shows the packet is serialized through
  `writeWithSerializationMode` reaching `serialize<ItemRegistryPacketPayload>::write` directly - `write` is a
  different virtual, so the hook never ran. It is now on the payload constructor, which is where `mItems` is
  produced and which every sending path goes through. Reading `components` out of an entry that has none also
  threw `bad variant access` and aborted the whole merge, so the node is only read after a check
- `ICustomItem` stopped setting `mItemParseVersion = ItemVersion::DataDriven`: it was commented out during an
  experiment and left that way. That flag is what gives an item the definition machinery a client needs, and
  custom items came up blank from then on (`Item <name> requires either an icon atlas or icon texture`)
- Re-registering an item name left the replaced item in `ItemRegistry::mItemRegistry`, and
  `CreativeItemRegistry` appended a creative entry per registration, so items showed up twice in a client's
  inventory and the leftover reported default fields. Registration now drops the replaced entry, and
  `registerCreativeItem` replaces the entry for an item instead of appending another
- `/give <custom item>` silently did nothing: the item enum entry was written as `CommandItem{id, true, false}`,
  but the type is a union of `{ short mVersion; bool mOverrideAux; int mId; }`. `CommandItem::createInstance`
  only reads `mId` as the item's own id when `mVersion` is non-zero - with 0 it converts the value as a *legacy*
  id through a different table
- A block item's id has to be the block's id folded into a short (`id <= 0xFF ? id : 255 - id`), which is what
  `ItemRegistryRef::registerBlockItem<BlockItem>` hands to `BlockItem`'s constructor. Assigning an id afterwards
  left the item's own state disagreeing with `mId`, which showed up as a creative entry nothing could be done
  with
- A block whose type a mod registers in C++ is not listed by the engine at all: the engine creates items for the
  blocks it parses out of documents, so that item is queued for the creative pass instead. The queue cannot
  duplicate anything now that `registerCreativeItem` replaces

- A block item drawn as the block itself (rather than as a flat picture) needs the `minecraft:block_placer`
  component in its definition, with the block it stands for. Two details decide whether a client acts on it, and
  both come out of `PlanterItemComponent::buildNetworkTag` in the old build, which writes `canUseBlockAsIcon` from
  the component's byte +64 and `replaceBlockItem` from +65: the keys are **camelCase** in the network form (a
  *document* uses `replace_block_item`, bound by `bindType`), and `canUseBlockAsIcon` has no document key at all -
  the definition sent to a client is the only place it can be set. A `minecraft:icon` must be left out at the same
  time: `ComponentItem::getIconInfo` reads the icon component first and only falls back to the item's block when
  there is none, so writing an icon pins the item to a texture

- A block registered from C++ now reaches clients without any document. The engine builds the list a client is
  told about (`StartGamePacketPayload::mBlockProperties`) out of the block definition group, and a block whose type
  ModAPI registered itself is not in that group - so the packet's write path adds the entry it is missing. The entry
  is field for field the one the engine writes itself (read out of a live one as SNBT), which is why it works: the
  same `components`/`minecraft:material_instances`, `menu_category`, `molangVersion` and `vanilla_block_data`, with
  the block's own id. `ServerBlockProperty`'s default constructor is declared in the header but not exported, so its
  definition is written here.
- The document path is gone with it: `registerBlockFromMemoryJson`, `registerBlockFromJsonFile`, `deferBlockDocument`
  and the runtime pack they installed were removed, along with the suppression of the engine's block type and the
  bookkeeping that went with it. A block is a `::BlockType` subclass registered under an identifier, and the texture
  a client draws it with is named with `BlockRegistry::setBlockTexture` - the texture file and the resource pack
  mappings (`blocks.json`, `terrain_texture.json`) are the mod's own, exactly as they are for an addon block.
- `ICustomBlock` is gone as well: it only forwarded a constructor to `::BlockType`, so a mod derives from
  `::BlockType` directly. `ICustomFeature` moved to `include/modapi/worldgen/base/` and
  `src/modapi/worldgen/base/`, which is where every other registry keeps its interfaces.

- The definition-level part of a block entry comes out of the registration's own `BlockDescription` instead of
  defaults: `menu_category.category` through the engine's exported
  `SharedTypes::v1_21_110::ItemCategory::stringFromCreativeItemCategory`, `group` and `is_hidden_in_commands` from
  `BlockMenuCategory`, and `vanilla_block_data.block_id` / `material` from `VanillaBlockData` (the material name
  through `BlockEnum::MaterialTypeToString`). `BlockPermutationDescription`s are published as `permutations`, each
  with its `condition` (`ExpressionNode::getExpressionString`), its components and its tags - the same three nodes the
  engine writes.
- A block is registered with what to tell a client about it, in one call and in the engine's own types:
  `registerBlock<MyBlock>("mymod:my_block", { .mArguments = {...}, .mProperty = {...} })`, with `BlockProperty` made
  of `BlockComponentGroupDescription` / `BlockDescription` / `BlockPermutationDescription`. The same property drives
  both sides: its components are injected into the running block type through `initializeComponentFromCode` - the
  route vanilla uses for its own blocks - and serialised for a client through `isNetworkComponent` + `getName` +
  `buildNetworkTag`, which are the calls `generateServerBlockProperties` makes. The entry produced this way matches
  the engine's own field for field (measured against the entry the engine writes for an addon block: identical apart
  from each block's own texture name and id). No `setBlockTexture`/`setBlockProperty`: a property is handed over with
  the registration.
- The engine declares default constructors and copy operations for those definition types and does not export them,
  so `include/modapi/block/base/EngineDefaults.h` supplies the definitions inline - default constructors only, and
  copy/assignment only where the linker proved them missing. A type holding a `CompoundTag`
  (`ServerBlockProperty`) keeps the engine's exported copy: a memberwise `= default` there copies the tag shallowly
  and two objects then free the same storage, which crashed the server at `StartGamePacket` time.

### Changed

- Adapted LeviLamina 1.6.x and BDS 1.21.111.1 @zimuya4153

## [0.1.1] - 2025-10-12

### Fixed

- Fixed the Chinese path and incorrect output of AddonLoader [#1] @zimuya4153

## [0.1.0] - 2025-10-04

- The test ships a real 16x16 PNG for its block texture (embedded, generated with Pillow, also kept at `src-test/assets/modapi_test_block.png`) instead of a placeholder file, and asserts the shipped file carries the PNG signature: a placeholder leaves a client with nothing to draw, which was a real defect spotted in review

### Added

- `modapi::item::ICustomBlockItem` (`include/modapi/item/base/ICustomBlockItem.h`): the item that places a custom block. It derives from the engine's `::BlockItem` and is registered through `ItemRegistry` like any other item (`isEntry` already accepted `std::derived_from<Entry, ::Item>`, so nothing had to be loosened), with an `initCustomItem(ICustomBlockItem&)` overload next to the existing `ICustomItem`/`ICustomArmorItem` ones. Measured: `::BlockItem` does *not* link itself to its block (`getBlockTypeForRendering` is not even overridden by it) - the link is `::Item::mBlockType`, so the class sets it in `_init`; before that an item built for a custom block reported `minecraft:air`, afterwards it reports the block. Registration timing needed no change either: block types are registered right after the level reads its definitions, which is measured to be before vanilla builds its items, so a block item finds its block; an earlier attempt to move block registration into the item pass was reverted, along with the switch to a ModAPI owned id range
- `src-test/BlockTest.cpp` overrides virtuals on both sides and asserts the effect: the block item's engine fields (max stack size 16, foil), an engine virtual (`isMusicDisk`), the item -> block link (`mBlockType`), and a block hook (`canProvideSupport`). A plain `ICustomItem`'s overrides are only *recorded*, with the reason: its engine object is replaced by the engine in a later pass (measured: the registry holds a different pointer than the one ModAPI built), so its fields read the defaults - a pre-existing ModAPI issue, out of scope here. Note also that this build has no RTTI data, so `dynamic_cast` aborts ("no RTTI data") and a `static_cast` on a pointer you own is the way to go

- Block items now send a client definition of their own: `NetworkTagBuilder` gained a `buildClientComponents(ICustomBlockItem const&)` overload and `ICustomBlockItem` gained `getIcon`, `getDisplayName` and a `buildNetworkTag` override. Without it the creative entry for a block item had no texture (the block itself rendered, so the packs were delivered - the item definition simply had no icon) and the name stayed an untranslated key. Measured after the change: the item tag carries the texture name (`blockItem.tagHasIcon`). The test also names its block item exactly like its block, as vanilla does (`minecraft:stone` is both), which the engine's block -> item resolution for pick block relies on, and its resource pack now ships `texts/en_US.lang` / `texts/zh_CN.lang`

### Fixed

- The vanilla `minecraft:stick` losing its icon on a client (`Missing icon for data-driven item 'minecraft:stick'`) was caused by this repository's own `[REGISTRY]` suite: it exercised the vanilla item setters (`setIcon`, `setDisplayName`, `addTag`, `setFireResistant`, `setRepairItem`, `getAndModifyVanillaNetworkTagInfo`) on `minecraft:stick`, which fills `mModifiedVanillaItems` and makes `VanillaItemDefinitionSendHook` replace that item's `components` in the registry packet. The suite now modifies the item it registers itself (`modapi_test:test_item`) and only reads the vanilla one. Measured: with both mods loaded the stick's definition is no longer touched, and all four suites stay green. It was *not* the block item, the runtime packs, `ItemVersion::DataDriven`, item id collisions or the creative pass - each of those was ruled out by an A/B, several of which were invalid until the manifest explained why: `ModAPI` is `"passive": true` and `test` is its only dependent, so renaming `test` away also unloaded ModAPI (LeviLamina reported "loaded 0 mods"), which is what made "disable the test mod" look like a fix
- Follow-up to that, and the reason it was so damaging: `VanillaItemDefinitionSendHook` used to *replace* an item's whole `components` node with the tag recorded by the setters, and that tag is built from `Item::buildNetworkTag()` - for a data driven vanilla item nearly empty (measured: `minecraft:stick` came out as 37 chars holding only `minecraft:hand_equipped`). So modifying such an item dropped everything it had. The hook now *merges* the recorded changes into what the engine already publishes for the item (top level, and inside `item_properties`), which keeps the data the mod did not touch - the icon of a modified item, for instance

- Re-registering an item name left the replaced item in `ItemRegistry::mItemRegistry`: the new item was appended while the old one stayed in the vector, so every pass that walks it (the client definition build, and this API's own `forEachItemInRegistry`) saw two items answering to one name. The leftover carried none of the values its mod had applied (`max stack size` 64 instead of 16), and the object handed back by the registration differed from the one the registry exposed. Registration now drops the replaced entry, and a custom item's `_init` values survive - verified server side: `plain item fields: stack = 16, glint = true`, and the registry object and the registration product are the same pointer

- `/give <custom item>` silently produced nothing: the enum entry was written as `CommandItem{id, true, false}`, but the type is a union of `{ short mVersion; bool mOverrideAux; int mId; }` over a `uint64`. The id therefore landed in `mVersion` (truncated) and `mId` was left 0. Reading the engine's own `CommandItem::createInstance` settled the rest: it only treats `mId` as the item's own id when `mVersion` is **non-zero** - with 0 it converts the value as a *legacy* id through a different table, which is why an entry for `minecraft:stick` resolved to `minecraft:oak_sign` while our item resolved to nothing. The entry is now `{1, true, id}`. The registry suite builds that value and asserts `createInstance` returns the item (`give.*`)
- Note for whoever tests this next: the item must be reached by a **real** player - a virtual one gives `No targets matched selector`; and an item whose identifier contains a dot (`modapi_test:item.test_block`) is rejected by the command parser outright, so a block item registered under that name cannot be given at all

- The definition packet a client receives was never filled in for custom items. `ModAPI` hooked
  `ItemRegistryPacket::write`, but the old build shows the packet is serialized through
  `writeWithSerializationMode` reaching `serialize<ItemRegistryPacketPayload>::write` directly - `write` is a
  different virtual, so the hook never ran. It is now on the payload constructor, which is where `mItems` is
  produced and which every sending path goes through. Reading `components` out of an entry that has none also
  threw `bad variant access` and aborted the whole merge, so the node is only read after a check
- `ICustomItem` stopped setting `mItemParseVersion = ItemVersion::DataDriven`: it was commented out during an
  experiment and left that way. That flag is what gives an item the definition machinery a client needs, and
  custom items came up blank from then on (`Item <name> requires either an icon atlas or icon texture`)
- Re-registering an item name left the replaced item in `ItemRegistry::mItemRegistry`, and
  `CreativeItemRegistry` appended a creative entry per registration, so items showed up twice in a client's
  inventory and the leftover reported default fields. Registration now drops the replaced entry, and
  `registerCreativeItem` replaces the entry for an item instead of appending another
- `/give <custom item>` silently did nothing: the item enum entry was written as `CommandItem{id, true, false}`,
  but the type is a union of `{ short mVersion; bool mOverrideAux; int mId; }`. `CommandItem::createInstance`
  only reads `mId` as the item's own id when `mVersion` is non-zero - with 0 it converts the value as a *legacy*
  id through a different table
- A block item's id has to be the block's id folded into a short (`id <= 0xFF ? id : 255 - id`), which is what
  `ItemRegistryRef::registerBlockItem<BlockItem>` hands to `BlockItem`'s constructor. Assigning an id afterwards
  left the item's own state disagreeing with `mId`, which showed up as a creative entry nothing could be done
  with
- A block whose type a mod registers in C++ is not listed by the engine at all: the engine creates items for the
  blocks it parses out of documents, so that item is queued for the creative pass instead. The queue cannot
  duplicate anything now that `registerCreativeItem` replaces

- A block item drawn as the block itself (rather than as a flat picture) needs the `minecraft:block_placer`
  component in its definition, with the block it stands for. Two details decide whether a client acts on it, and
  both come out of `PlanterItemComponent::buildNetworkTag` in the old build, which writes `canUseBlockAsIcon` from
  the component's byte +64 and `replaceBlockItem` from +65: the keys are **camelCase** in the network form (a
  *document* uses `replace_block_item`, bound by `bindType`), and `canUseBlockAsIcon` has no document key at all -
  the definition sent to a client is the only place it can be set. A `minecraft:icon` must be left out at the same
  time: `ComponentItem::getIconInfo` reads the icon component first and only falls back to the item's block when
  there is none, so writing an icon pins the item to a texture

- A block registered from C++ now reaches clients without any document. The engine builds the list a client is
  told about (`StartGamePacketPayload::mBlockProperties`) out of the block definition group, and a block whose type
  ModAPI registered itself is not in that group - so the packet's write path adds the entry it is missing. The entry
  is field for field the one the engine writes itself (read out of a live one as SNBT), which is why it works: the
  same `components`/`minecraft:material_instances`, `menu_category`, `molangVersion` and `vanilla_block_data`, with
  the block's own id. `ServerBlockProperty`'s default constructor is declared in the header but not exported, so its
  definition is written here.
- The document path is gone with it: `registerBlockFromMemoryJson`, `registerBlockFromJsonFile`, `deferBlockDocument`
  and the runtime pack they installed were removed, along with the suppression of the engine's block type and the
  bookkeeping that went with it. A block is a `::BlockType` subclass registered under an identifier, and the texture
  a client draws it with is named with `BlockRegistry::setBlockTexture` - the texture file and the resource pack
  mappings (`blocks.json`, `terrain_texture.json`) are the mod's own, exactly as they are for an addon block.
- `ICustomBlock` is gone as well: it only forwarded a constructor to `::BlockType`, so a mod derives from
  `::BlockType` directly. `ICustomFeature` moved to `include/modapi/worldgen/base/` and
  `src/modapi/worldgen/base/`, which is where every other registry keeps its interfaces.

- The definition-level part of a block entry comes out of the registration's own `BlockDescription` instead of
  defaults: `menu_category.category` through the engine's exported
  `SharedTypes::v1_21_110::ItemCategory::stringFromCreativeItemCategory`, `group` and `is_hidden_in_commands` from
  `BlockMenuCategory`, and `vanilla_block_data.block_id` / `material` from `VanillaBlockData` (the material name
  through `BlockEnum::MaterialTypeToString`). `BlockPermutationDescription`s are published as `permutations`, each
  with its `condition` (`ExpressionNode::getExpressionString`), its components and its tags - the same three nodes the
  engine writes.
- A block is registered with what to tell a client about it, in one call and in the engine's own types:
  `registerBlock<MyBlock>("mymod:my_block", { .mArguments = {...}, .mProperty = {...} })`, with `BlockProperty` made
  of `BlockComponentGroupDescription` / `BlockDescription` / `BlockPermutationDescription`. The same property drives
  both sides: its components are injected into the running block type through `initializeComponentFromCode` - the
  route vanilla uses for its own blocks - and serialised for a client through `isNetworkComponent` + `getName` +
  `buildNetworkTag`, which are the calls `generateServerBlockProperties` makes. The entry produced this way matches
  the engine's own field for field (measured against the entry the engine writes for an addon block: identical apart
  from each block's own texture name and id). No `setBlockTexture`/`setBlockProperty`: a property is handed over with
  the registration.
- The engine declares default constructors and copy operations for those definition types and does not export them,
  so `include/modapi/block/base/EngineDefaults.h` supplies the definitions inline - default constructors only, and
  copy/assignment only where the linker proved them missing. A type holding a `CompoundTag`
  (`ServerBlockProperty`) keeps the engine's exported copy: a memberwise `= default` there copies the tag shallowly
  and two objects then free the same storage, which crashed the server at `StartGamePacket` time.

### Changed

- `RuntimePack` gets a fresh uuid on every construction instead of a name derived one (and logs it): a client that already has a pack with the same uuid does not download it again, so a pack rebuilt on each server start has to offer an identity no client has seen. Packs also have to be installed while mods load - the stack clients are offered is composed when a level starts, so a pack installed later (from a command, for instance) never reaches them; the test installs its packs there now

- The test ships a real 16x16 PNG for its block texture (embedded, generated with Pillow, also kept at `src-test/assets/modapi_test_block.png`) instead of a placeholder file, and asserts the shipped file carries the PNG signature: a placeholder leaves a client with nothing to draw, which was a real defect spotted in review

### Added

- `modapi::item::ICustomBlockItem` (`include/modapi/item/base/ICustomBlockItem.h`): the item that places a custom block. It derives from the engine's `::BlockItem` and is registered through `ItemRegistry` like any other item (`isEntry` already accepted `std::derived_from<Entry, ::Item>`, so nothing had to be loosened), with an `initCustomItem(ICustomBlockItem&)` overload next to the existing `ICustomItem`/`ICustomArmorItem` ones. Measured: `::BlockItem` does *not* link itself to its block (`getBlockTypeForRendering` is not even overridden by it) - the link is `::Item::mBlockType`, so the class sets it in `_init`; before that an item built for a custom block reported `minecraft:air`, afterwards it reports the block. Registration timing needed no change either: block types are registered right after the level reads its definitions, which is measured to be before vanilla builds its items, so a block item finds its block; an earlier attempt to move block registration into the item pass was reverted, along with the switch to a ModAPI owned id range
- `src-test/BlockTest.cpp` overrides virtuals on both sides and asserts the effect: the block item's engine fields (max stack size 16, foil), an engine virtual (`isMusicDisk`), the item -> block link (`mBlockType`), and a block hook (`canProvideSupport`). A plain `ICustomItem`'s overrides are only *recorded*, with the reason: its engine object is replaced by the engine in a later pass (measured: the registry holds a different pointer than the one ModAPI built), so its fields read the defaults - a pre-existing ModAPI issue, out of scope here. Note also that this build has no RTTI data, so `dynamic_cast` aborts ("no RTTI data") and a `static_cast` on a pointer you own is the way to go

- Block items now send a client definition of their own: `NetworkTagBuilder` gained a `buildClientComponents(ICustomBlockItem const&)` overload and `ICustomBlockItem` gained `getIcon`, `getDisplayName` and a `buildNetworkTag` override. Without it the creative entry for a block item had no texture (the block itself rendered, so the packs were delivered - the item definition simply had no icon) and the name stayed an untranslated key. Measured after the change: the item tag carries the texture name (`blockItem.tagHasIcon`). The test also names its block item exactly like its block, as vanilla does (`minecraft:stone` is both), which the engine's block -> item resolution for pick block relies on, and its resource pack now ships `texts/en_US.lang` / `texts/zh_CN.lang`

### Fixed

- The vanilla `minecraft:stick` losing its icon on a client (`Missing icon for data-driven item 'minecraft:stick'`) was caused by this repository's own `[REGISTRY]` suite: it exercised the vanilla item setters (`setIcon`, `setDisplayName`, `addTag`, `setFireResistant`, `setRepairItem`, `getAndModifyVanillaNetworkTagInfo`) on `minecraft:stick`, which fills `mModifiedVanillaItems` and makes `VanillaItemDefinitionSendHook` replace that item's `components` in the registry packet. The suite now modifies the item it registers itself (`modapi_test:test_item`) and only reads the vanilla one. Measured: with both mods loaded the stick's definition is no longer touched, and all four suites stay green. It was *not* the block item, the runtime packs, `ItemVersion::DataDriven`, item id collisions or the creative pass - each of those was ruled out by an A/B, several of which were invalid until the manifest explained why: `ModAPI` is `"passive": true` and `test` is its only dependent, so renaming `test` away also unloaded ModAPI (LeviLamina reported "loaded 0 mods"), which is what made "disable the test mod" look like a fix
- Follow-up to that, and the reason it was so damaging: `VanillaItemDefinitionSendHook` used to *replace* an item's whole `components` node with the tag recorded by the setters, and that tag is built from `Item::buildNetworkTag()` - for a data driven vanilla item nearly empty (measured: `minecraft:stick` came out as 37 chars holding only `minecraft:hand_equipped`). So modifying such an item dropped everything it had. The hook now *merges* the recorded changes into what the engine already publishes for the item (top level, and inside `item_properties`), which keeps the data the mod did not touch - the icon of a modified item, for instance

- Re-registering an item name left the replaced item in `ItemRegistry::mItemRegistry`: the new item was appended while the old one stayed in the vector, so every pass that walks it (the client definition build, and this API's own `forEachItemInRegistry`) saw two items answering to one name. The leftover carried none of the values its mod had applied (`max stack size` 64 instead of 16), and the object handed back by the registration differed from the one the registry exposed. Registration now drops the replaced entry, and a custom item's `_init` values survive - verified server side: `plain item fields: stack = 16, glint = true`, and the registry object and the registration product are the same pointer

- `/give <custom item>` silently produced nothing: the enum entry was written as `CommandItem{id, true, false}`, but the type is a union of `{ short mVersion; bool mOverrideAux; int mId; }` over a `uint64`. The id therefore landed in `mVersion` (truncated) and `mId` was left 0. Reading the engine's own `CommandItem::createInstance` settled the rest: it only treats `mId` as the item's own id when `mVersion` is **non-zero** - with 0 it converts the value as a *legacy* id through a different table, which is why an entry for `minecraft:stick` resolved to `minecraft:oak_sign` while our item resolved to nothing. The entry is now `{1, true, id}`. The registry suite builds that value and asserts `createInstance` returns the item (`give.*`)
- Note for whoever tests this next: the item must be reached by a **real** player - a virtual one gives `No targets matched selector`; and an item whose identifier contains a dot (`modapi_test:item.test_block`) is rejected by the command parser outright, so a block item registered under that name cannot be given at all

- The definition packet a client receives was never filled in for custom items. `ModAPI` hooked
  `ItemRegistryPacket::write`, but the old build shows the packet is serialized through
  `writeWithSerializationMode` reaching `serialize<ItemRegistryPacketPayload>::write` directly - `write` is a
  different virtual, so the hook never ran. It is now on the payload constructor, which is where `mItems` is
  produced and which every sending path goes through. Reading `components` out of an entry that has none also
  threw `bad variant access` and aborted the whole merge, so the node is only read after a check
- `ICustomItem` stopped setting `mItemParseVersion = ItemVersion::DataDriven`: it was commented out during an
  experiment and left that way. That flag is what gives an item the definition machinery a client needs, and
  custom items came up blank from then on (`Item <name> requires either an icon atlas or icon texture`)
- Re-registering an item name left the replaced item in `ItemRegistry::mItemRegistry`, and
  `CreativeItemRegistry` appended a creative entry per registration, so items showed up twice in a client's
  inventory and the leftover reported default fields. Registration now drops the replaced entry, and
  `registerCreativeItem` replaces the entry for an item instead of appending another
- `/give <custom item>` silently did nothing: the item enum entry was written as `CommandItem{id, true, false}`,
  but the type is a union of `{ short mVersion; bool mOverrideAux; int mId; }`. `CommandItem::createInstance`
  only reads `mId` as the item's own id when `mVersion` is non-zero - with 0 it converts the value as a *legacy*
  id through a different table
- A block item's id has to be the block's id folded into a short (`id <= 0xFF ? id : 255 - id`), which is what
  `ItemRegistryRef::registerBlockItem<BlockItem>` hands to `BlockItem`'s constructor. Assigning an id afterwards
  left the item's own state disagreeing with `mId`, which showed up as a creative entry nothing could be done
  with
- A block whose type a mod registers in C++ is not listed by the engine at all: the engine creates items for the
  blocks it parses out of documents, so that item is queued for the creative pass instead. The queue cannot
  duplicate anything now that `registerCreativeItem` replaces

- A block item drawn as the block itself (rather than as a flat picture) needs the `minecraft:block_placer`
  component in its definition, with the block it stands for. Two details decide whether a client acts on it, and
  both come out of `PlanterItemComponent::buildNetworkTag` in the old build, which writes `canUseBlockAsIcon` from
  the component's byte +64 and `replaceBlockItem` from +65: the keys are **camelCase** in the network form (a
  *document* uses `replace_block_item`, bound by `bindType`), and `canUseBlockAsIcon` has no document key at all -
  the definition sent to a client is the only place it can be set. A `minecraft:icon` must be left out at the same
  time: `ComponentItem::getIconInfo` reads the icon component first and only falls back to the item's block when
  there is none, so writing an icon pins the item to a texture

- A block registered from C++ now reaches clients without any document. The engine builds the list a client is
  told about (`StartGamePacketPayload::mBlockProperties`) out of the block definition group, and a block whose type
  ModAPI registered itself is not in that group - so the packet's write path adds the entry it is missing. The entry
  is field for field the one the engine writes itself (read out of a live one as SNBT), which is why it works: the
  same `components`/`minecraft:material_instances`, `menu_category`, `molangVersion` and `vanilla_block_data`, with
  the block's own id. `ServerBlockProperty`'s default constructor is declared in the header but not exported, so its
  definition is written here.
- The document path is gone with it: `registerBlockFromMemoryJson`, `registerBlockFromJsonFile`, `deferBlockDocument`
  and the runtime pack they installed were removed, along with the suppression of the engine's block type and the
  bookkeeping that went with it. A block is a `::BlockType` subclass registered under an identifier, and the texture
  a client draws it with is named with `BlockRegistry::setBlockTexture` - the texture file and the resource pack
  mappings (`blocks.json`, `terrain_texture.json`) are the mod's own, exactly as they are for an addon block.
- `ICustomBlock` is gone as well: it only forwarded a constructor to `::BlockType`, so a mod derives from
  `::BlockType` directly. `ICustomFeature` moved to `include/modapi/worldgen/base/` and
  `src/modapi/worldgen/base/`, which is where every other registry keeps its interfaces.

- The definition-level part of a block entry comes out of the registration's own `BlockDescription` instead of
  defaults: `menu_category.category` through the engine's exported
  `SharedTypes::v1_21_110::ItemCategory::stringFromCreativeItemCategory`, `group` and `is_hidden_in_commands` from
  `BlockMenuCategory`, and `vanilla_block_data.block_id` / `material` from `VanillaBlockData` (the material name
  through `BlockEnum::MaterialTypeToString`). `BlockPermutationDescription`s are published as `permutations`, each
  with its `condition` (`ExpressionNode::getExpressionString`), its components and its tags - the same three nodes the
  engine writes.
- A block is registered with what to tell a client about it, in one call and in the engine's own types:
  `registerBlock<MyBlock>("mymod:my_block", { .mArguments = {...}, .mProperty = {...} })`, with `BlockProperty` made
  of `BlockComponentGroupDescription` / `BlockDescription` / `BlockPermutationDescription`. The same property drives
  both sides: its components are injected into the running block type through `initializeComponentFromCode` - the
  route vanilla uses for its own blocks - and serialised for a client through `isNetworkComponent` + `getName` +
  `buildNetworkTag`, which are the calls `generateServerBlockProperties` makes. The entry produced this way matches
  the engine's own field for field (measured against the entry the engine writes for an addon block: identical apart
  from each block's own texture name and id). No `setBlockTexture`/`setBlockProperty`: a property is handed over with
  the registration.
- The engine declares default constructors and copy operations for those definition types and does not export them,
  so `include/modapi/block/base/EngineDefaults.h` supplies the definitions inline - default constructors only, and
  copy/assignment only where the linker proved them missing. A type holding a `CompoundTag`
  (`ServerBlockProperty`) keeps the engine's exported copy: a memberwise `= default` there copies the tag shallowly
  and two objects then free the same storage, which crashed the server at `StartGamePacket` time.

### Changed

- Block definitions come from JSON now, and the C++ component builders are gone: `modapi::block::setMaterial`, `setGeometry` and `ICustomBlock::setupDefinition` were removed. A block's behaviour stays C++ (the mod's `ICustomBlock`, which is the `::BlockType` the engine uses), while its definition is an addon style document handed to `modapi::block::registerBlockFromJson`. That document is shipped as a behaviour pack through `RuntimePack`/`AddonsLoader`, so the engine's own parser turns it into a `BlockDefinition` while a level loads its block definitions (the `PackLoadContext` those parsers want cannot be constructed from a mod: its default constructor is not exported). The step where the engine would register its own block type for the document is hooked away for documents ModAPI knows, because registering the same identifier from C++ as well crashed the dedicated server (0xC0000005, reproduced twice: the registry keeps the existing entry and silently drops the new object). With the hook, the C++ object is the registered type and clients get the JSON definition - asserted by pointer equality and by the material component appearing in the definition. A code registration whose identifier is already defined by a pack ModAPI does not own is refused with an error rather than left inconsistent

### Added

- `modapi::addons::RuntimePack` (base `AddonsLoader`, which already existed): build any client-facing pack at runtime - arbitrary JSON and binary files, `"resources"` or `"data"` module - written into a random directory under the system temp directory and handed to the engine through `AddonsLoader::addCustomPackPath`, i.e. the same directory-pack-source path the engine loads behaviour and resource packs with, so the packs end up in the stack clients are sent. Nothing is written into the server directory or a world\'s pack list. Deliberately not specialised for blocks: a block\'s texture and definition are just files a mod adds (the test does exactly that) - the block-specific wrapper that briefly existed was removed on request
- `src-test/BlockTest.cpp` (`modapitest blocks`) installs three packs through that abstraction (a plain resource pack, a block definition pack and a texture pack), asserting the temp layout, the manifest and the copied png. What cannot be checked here is whether a real client accepts them: there is no client in this environment

- Components for custom blocks: `ICustomBlock::setupDefinition(::BlockDefinition&)` is called while a block is registered (the definition exists by then), and `modapi::block::setMaterial(definition, textureName, renderLayer, tintMethod)` adds a `minecraft:material_instances` component through the engine's own code constructor (`BlockMaterialInstancesDescription`, `MCAPI`), no `PackLoadContext` needed. Measured: material instances are *not* network components, so they do not appear in the tag `generateServerBlockProperties` builds - that tag carries the block name and the behavioural network components, while rendering data comes from the client's resource pack, exactly like an addon block. `setGeometry` is deliberately absent: `BlockGeometryDescription` derives from the template `NetworkedBlockComponentDescription<T>`, which is an empty stub in this build's headers, so it cannot be converted to a `BlockComponentDescription` (fixable by supplying the missing template in `src/mc/...`)
- `src-test/BlockTest.cpp` asserts the material reaches the block's definition (`definition.material`, `definition.material.hasTexture`)

- `modapi::block::BlockRegistry` and `modapi::block::ICustomBlock` (`include/modapi/block/`): a custom block is an engine block type (the mod derives from `ICustomBlock`, itself a `::BlockType`), registered through vanilla's own `BlockTypeRegistry::registerBlock` right after `Level::loadBlockDefinitionGroup` ran - the moment vanilla and every behaviour pack registered their definitions, and early enough that the registry still builds block states and the properties clients are told about. The id comes from `BlockDefinitionGroup::mLastBlockId` (10001 and up in this build); `BlockReadyEvent` and `DeferredRegister<BlockRegistry, ...>` work like the other registries, and the states only exist after the registry finalised, so `getDefaultBlockState` is meaningful once the server started. The registry also adds a matching `BlockDefinition` to the level's definition group, so `generateServerBlockProperties` - the list `StartGamePacket` sends - reports the block (asserted by the test); its look still comes from a client resource pack, exactly like an addon block, and whether a real client accepts it is not verifiable here
- `src-test/BlockTest.cpp` (`modapitest blocks`): registration, lookup, "the block has a default state" (the check that says it can be placed, plus a vanilla control), and a world case that places the block through `BlockHelper` in a virtual player's chunk and reads it back - skipped, and counted as such, when the environment has no loaded chunk
- `[BLOCKHELPER]` now reports every chunk dependent case as `skipped=` instead of only logging a warning, so its report says how many cases really ran

- Complete the separation of the ModAPI library @zimuya4153

- The test ships a real 16x16 PNG for its block texture (embedded, generated with Pillow, also kept at `src-test/assets/modapi_test_block.png`) instead of a placeholder file, and asserts the shipped file carries the PNG signature: a placeholder leaves a client with nothing to draw, which was a real defect spotted in review

### Added

- `modapi::item::ICustomBlockItem` (`include/modapi/item/base/ICustomBlockItem.h`): the item that places a custom block. It derives from the engine's `::BlockItem` and is registered through `ItemRegistry` like any other item (`isEntry` already accepted `std::derived_from<Entry, ::Item>`, so nothing had to be loosened), with an `initCustomItem(ICustomBlockItem&)` overload next to the existing `ICustomItem`/`ICustomArmorItem` ones. Measured: `::BlockItem` does *not* link itself to its block (`getBlockTypeForRendering` is not even overridden by it) - the link is `::Item::mBlockType`, so the class sets it in `_init`; before that an item built for a custom block reported `minecraft:air`, afterwards it reports the block. Registration timing needed no change either: block types are registered right after the level reads its definitions, which is measured to be before vanilla builds its items, so a block item finds its block; an earlier attempt to move block registration into the item pass was reverted, along with the switch to a ModAPI owned id range
- `src-test/BlockTest.cpp` overrides virtuals on both sides and asserts the effect: the block item's engine fields (max stack size 16, foil), an engine virtual (`isMusicDisk`), the item -> block link (`mBlockType`), and a block hook (`canProvideSupport`). A plain `ICustomItem`'s overrides are only *recorded*, with the reason: its engine object is replaced by the engine in a later pass (measured: the registry holds a different pointer than the one ModAPI built), so its fields read the defaults - a pre-existing ModAPI issue, out of scope here. Note also that this build has no RTTI data, so `dynamic_cast` aborts ("no RTTI data") and a `static_cast` on a pointer you own is the way to go

- Block items now send a client definition of their own: `NetworkTagBuilder` gained a `buildClientComponents(ICustomBlockItem const&)` overload and `ICustomBlockItem` gained `getIcon`, `getDisplayName` and a `buildNetworkTag` override. Without it the creative entry for a block item had no texture (the block itself rendered, so the packs were delivered - the item definition simply had no icon) and the name stayed an untranslated key. Measured after the change: the item tag carries the texture name (`blockItem.tagHasIcon`). The test also names its block item exactly like its block, as vanilla does (`minecraft:stone` is both), which the engine's block -> item resolution for pick block relies on, and its resource pack now ships `texts/en_US.lang` / `texts/zh_CN.lang`

### Fixed

- The vanilla `minecraft:stick` losing its icon on a client (`Missing icon for data-driven item 'minecraft:stick'`) was caused by this repository's own `[REGISTRY]` suite: it exercised the vanilla item setters (`setIcon`, `setDisplayName`, `addTag`, `setFireResistant`, `setRepairItem`, `getAndModifyVanillaNetworkTagInfo`) on `minecraft:stick`, which fills `mModifiedVanillaItems` and makes `VanillaItemDefinitionSendHook` replace that item's `components` in the registry packet. The suite now modifies the item it registers itself (`modapi_test:test_item`) and only reads the vanilla one. Measured: with both mods loaded the stick's definition is no longer touched, and all four suites stay green. It was *not* the block item, the runtime packs, `ItemVersion::DataDriven`, item id collisions or the creative pass - each of those was ruled out by an A/B, several of which were invalid until the manifest explained why: `ModAPI` is `"passive": true` and `test` is its only dependent, so renaming `test` away also unloaded ModAPI (LeviLamina reported "loaded 0 mods"), which is what made "disable the test mod" look like a fix
- Follow-up to that, and the reason it was so damaging: `VanillaItemDefinitionSendHook` used to *replace* an item's whole `components` node with the tag recorded by the setters, and that tag is built from `Item::buildNetworkTag()` - for a data driven vanilla item nearly empty (measured: `minecraft:stick` came out as 37 chars holding only `minecraft:hand_equipped`). So modifying such an item dropped everything it had. The hook now *merges* the recorded changes into what the engine already publishes for the item (top level, and inside `item_properties`), which keeps the data the mod did not touch - the icon of a modified item, for instance

- Re-registering an item name left the replaced item in `ItemRegistry::mItemRegistry`: the new item was appended while the old one stayed in the vector, so every pass that walks it (the client definition build, and this API's own `forEachItemInRegistry`) saw two items answering to one name. The leftover carried none of the values its mod had applied (`max stack size` 64 instead of 16), and the object handed back by the registration differed from the one the registry exposed. Registration now drops the replaced entry, and a custom item's `_init` values survive - verified server side: `plain item fields: stack = 16, glint = true`, and the registry object and the registration product are the same pointer

- `/give <custom item>` silently produced nothing: the enum entry was written as `CommandItem{id, true, false}`, but the type is a union of `{ short mVersion; bool mOverrideAux; int mId; }` over a `uint64`. The id therefore landed in `mVersion` (truncated) and `mId` was left 0. Reading the engine's own `CommandItem::createInstance` settled the rest: it only treats `mId` as the item's own id when `mVersion` is **non-zero** - with 0 it converts the value as a *legacy* id through a different table, which is why an entry for `minecraft:stick` resolved to `minecraft:oak_sign` while our item resolved to nothing. The entry is now `{1, true, id}`. The registry suite builds that value and asserts `createInstance` returns the item (`give.*`)
- Note for whoever tests this next: the item must be reached by a **real** player - a virtual one gives `No targets matched selector`; and an item whose identifier contains a dot (`modapi_test:item.test_block`) is rejected by the command parser outright, so a block item registered under that name cannot be given at all

- The definition packet a client receives was never filled in for custom items. `ModAPI` hooked
  `ItemRegistryPacket::write`, but the old build shows the packet is serialized through
  `writeWithSerializationMode` reaching `serialize<ItemRegistryPacketPayload>::write` directly - `write` is a
  different virtual, so the hook never ran. It is now on the payload constructor, which is where `mItems` is
  produced and which every sending path goes through. Reading `components` out of an entry that has none also
  threw `bad variant access` and aborted the whole merge, so the node is only read after a check
- `ICustomItem` stopped setting `mItemParseVersion = ItemVersion::DataDriven`: it was commented out during an
  experiment and left that way. That flag is what gives an item the definition machinery a client needs, and
  custom items came up blank from then on (`Item <name> requires either an icon atlas or icon texture`)
- Re-registering an item name left the replaced item in `ItemRegistry::mItemRegistry`, and
  `CreativeItemRegistry` appended a creative entry per registration, so items showed up twice in a client's
  inventory and the leftover reported default fields. Registration now drops the replaced entry, and
  `registerCreativeItem` replaces the entry for an item instead of appending another
- `/give <custom item>` silently did nothing: the item enum entry was written as `CommandItem{id, true, false}`,
  but the type is a union of `{ short mVersion; bool mOverrideAux; int mId; }`. `CommandItem::createInstance`
  only reads `mId` as the item's own id when `mVersion` is non-zero - with 0 it converts the value as a *legacy*
  id through a different table
- A block item's id has to be the block's id folded into a short (`id <= 0xFF ? id : 255 - id`), which is what
  `ItemRegistryRef::registerBlockItem<BlockItem>` hands to `BlockItem`'s constructor. Assigning an id afterwards
  left the item's own state disagreeing with `mId`, which showed up as a creative entry nothing could be done
  with
- A block whose type a mod registers in C++ is not listed by the engine at all: the engine creates items for the
  blocks it parses out of documents, so that item is queued for the creative pass instead. The queue cannot
  duplicate anything now that `registerCreativeItem` replaces

- A block item drawn as the block itself (rather than as a flat picture) needs the `minecraft:block_placer`
  component in its definition, with the block it stands for. Two details decide whether a client acts on it, and
  both come out of `PlanterItemComponent::buildNetworkTag` in the old build, which writes `canUseBlockAsIcon` from
  the component's byte +64 and `replaceBlockItem` from +65: the keys are **camelCase** in the network form (a
  *document* uses `replace_block_item`, bound by `bindType`), and `canUseBlockAsIcon` has no document key at all -
  the definition sent to a client is the only place it can be set. A `minecraft:icon` must be left out at the same
  time: `ComponentItem::getIconInfo` reads the icon component first and only falls back to the item's block when
  there is none, so writing an icon pins the item to a texture

- A block registered from C++ now reaches clients without any document. The engine builds the list a client is
  told about (`StartGamePacketPayload::mBlockProperties`) out of the block definition group, and a block whose type
  ModAPI registered itself is not in that group - so the packet's write path adds the entry it is missing. The entry
  is field for field the one the engine writes itself (read out of a live one as SNBT), which is why it works: the
  same `components`/`minecraft:material_instances`, `menu_category`, `molangVersion` and `vanilla_block_data`, with
  the block's own id. `ServerBlockProperty`'s default constructor is declared in the header but not exported, so its
  definition is written here.
- The document path is gone with it: `registerBlockFromMemoryJson`, `registerBlockFromJsonFile`, `deferBlockDocument`
  and the runtime pack they installed were removed, along with the suppression of the engine's block type and the
  bookkeeping that went with it. A block is a `::BlockType` subclass registered under an identifier, and the texture
  a client draws it with is named with `BlockRegistry::setBlockTexture` - the texture file and the resource pack
  mappings (`blocks.json`, `terrain_texture.json`) are the mod's own, exactly as they are for an addon block.
- `ICustomBlock` is gone as well: it only forwarded a constructor to `::BlockType`, so a mod derives from
  `::BlockType` directly. `ICustomFeature` moved to `include/modapi/worldgen/base/` and
  `src/modapi/worldgen/base/`, which is where every other registry keeps its interfaces.

- The definition-level part of a block entry comes out of the registration's own `BlockDescription` instead of
  defaults: `menu_category.category` through the engine's exported
  `SharedTypes::v1_21_110::ItemCategory::stringFromCreativeItemCategory`, `group` and `is_hidden_in_commands` from
  `BlockMenuCategory`, and `vanilla_block_data.block_id` / `material` from `VanillaBlockData` (the material name
  through `BlockEnum::MaterialTypeToString`). `BlockPermutationDescription`s are published as `permutations`, each
  with its `condition` (`ExpressionNode::getExpressionString`), its components and its tags - the same three nodes the
  engine writes.
- A block is registered with what to tell a client about it, in one call and in the engine's own types:
  `registerBlock<MyBlock>("mymod:my_block", { .mArguments = {...}, .mProperty = {...} })`, with `BlockProperty` made
  of `BlockComponentGroupDescription` / `BlockDescription` / `BlockPermutationDescription`. The same property drives
  both sides: its components are injected into the running block type through `initializeComponentFromCode` - the
  route vanilla uses for its own blocks - and serialised for a client through `isNetworkComponent` + `getName` +
  `buildNetworkTag`, which are the calls `generateServerBlockProperties` makes. The entry produced this way matches
  the engine's own field for field (measured against the entry the engine writes for an addon block: identical apart
  from each block's own texture name and id). No `setBlockTexture`/`setBlockProperty`: a property is handed over with
  the registration.
- The engine declares default constructors and copy operations for those definition types and does not export them,
  so `include/modapi/block/base/EngineDefaults.h` supplies the definitions inline - default constructors only, and
  copy/assignment only where the linker proved them missing. A type holding a `CompoundTag`
  (`ServerBlockProperty`) keeps the engine's exported copy: a memberwise `= default` there copies the tag shallowly
  and two objects then free the same storage, which crashed the server at `StartGamePacket` time.

### Changed

- `RuntimePack` gets a fresh uuid on every construction instead of a name derived one (and logs it): a client that already has a pack with the same uuid does not download it again, so a pack rebuilt on each server start has to offer an identity no client has seen. Packs also have to be installed while mods load - the stack clients are offered is composed when a level starts, so a pack installed later (from a command, for instance) never reaches them; the test installs its packs there now

- The test ships a real 16x16 PNG for its block texture (embedded, generated with Pillow, also kept at `src-test/assets/modapi_test_block.png`) instead of a placeholder file, and asserts the shipped file carries the PNG signature: a placeholder leaves a client with nothing to draw, which was a real defect spotted in review

### Added

- `modapi::item::ICustomBlockItem` (`include/modapi/item/base/ICustomBlockItem.h`): the item that places a custom block. It derives from the engine's `::BlockItem` and is registered through `ItemRegistry` like any other item (`isEntry` already accepted `std::derived_from<Entry, ::Item>`, so nothing had to be loosened), with an `initCustomItem(ICustomBlockItem&)` overload next to the existing `ICustomItem`/`ICustomArmorItem` ones. Measured: `::BlockItem` does *not* link itself to its block (`getBlockTypeForRendering` is not even overridden by it) - the link is `::Item::mBlockType`, so the class sets it in `_init`; before that an item built for a custom block reported `minecraft:air`, afterwards it reports the block. Registration timing needed no change either: block types are registered right after the level reads its definitions, which is measured to be before vanilla builds its items, so a block item finds its block; an earlier attempt to move block registration into the item pass was reverted, along with the switch to a ModAPI owned id range
- `src-test/BlockTest.cpp` overrides virtuals on both sides and asserts the effect: the block item's engine fields (max stack size 16, foil), an engine virtual (`isMusicDisk`), the item -> block link (`mBlockType`), and a block hook (`canProvideSupport`). A plain `ICustomItem`'s overrides are only *recorded*, with the reason: its engine object is replaced by the engine in a later pass (measured: the registry holds a different pointer than the one ModAPI built), so its fields read the defaults - a pre-existing ModAPI issue, out of scope here. Note also that this build has no RTTI data, so `dynamic_cast` aborts ("no RTTI data") and a `static_cast` on a pointer you own is the way to go

- Block items now send a client definition of their own: `NetworkTagBuilder` gained a `buildClientComponents(ICustomBlockItem const&)` overload and `ICustomBlockItem` gained `getIcon`, `getDisplayName` and a `buildNetworkTag` override. Without it the creative entry for a block item had no texture (the block itself rendered, so the packs were delivered - the item definition simply had no icon) and the name stayed an untranslated key. Measured after the change: the item tag carries the texture name (`blockItem.tagHasIcon`). The test also names its block item exactly like its block, as vanilla does (`minecraft:stone` is both), which the engine's block -> item resolution for pick block relies on, and its resource pack now ships `texts/en_US.lang` / `texts/zh_CN.lang`

### Fixed

- The vanilla `minecraft:stick` losing its icon on a client (`Missing icon for data-driven item 'minecraft:stick'`) was caused by this repository's own `[REGISTRY]` suite: it exercised the vanilla item setters (`setIcon`, `setDisplayName`, `addTag`, `setFireResistant`, `setRepairItem`, `getAndModifyVanillaNetworkTagInfo`) on `minecraft:stick`, which fills `mModifiedVanillaItems` and makes `VanillaItemDefinitionSendHook` replace that item's `components` in the registry packet. The suite now modifies the item it registers itself (`modapi_test:test_item`) and only reads the vanilla one. Measured: with both mods loaded the stick's definition is no longer touched, and all four suites stay green. It was *not* the block item, the runtime packs, `ItemVersion::DataDriven`, item id collisions or the creative pass - each of those was ruled out by an A/B, several of which were invalid until the manifest explained why: `ModAPI` is `"passive": true` and `test` is its only dependent, so renaming `test` away also unloaded ModAPI (LeviLamina reported "loaded 0 mods"), which is what made "disable the test mod" look like a fix
- Follow-up to that, and the reason it was so damaging: `VanillaItemDefinitionSendHook` used to *replace* an item's whole `components` node with the tag recorded by the setters, and that tag is built from `Item::buildNetworkTag()` - for a data driven vanilla item nearly empty (measured: `minecraft:stick` came out as 37 chars holding only `minecraft:hand_equipped`). So modifying such an item dropped everything it had. The hook now *merges* the recorded changes into what the engine already publishes for the item (top level, and inside `item_properties`), which keeps the data the mod did not touch - the icon of a modified item, for instance

- Re-registering an item name left the replaced item in `ItemRegistry::mItemRegistry`: the new item was appended while the old one stayed in the vector, so every pass that walks it (the client definition build, and this API's own `forEachItemInRegistry`) saw two items answering to one name. The leftover carried none of the values its mod had applied (`max stack size` 64 instead of 16), and the object handed back by the registration differed from the one the registry exposed. Registration now drops the replaced entry, and a custom item's `_init` values survive - verified server side: `plain item fields: stack = 16, glint = true`, and the registry object and the registration product are the same pointer

- `/give <custom item>` silently produced nothing: the enum entry was written as `CommandItem{id, true, false}`, but the type is a union of `{ short mVersion; bool mOverrideAux; int mId; }` over a `uint64`. The id therefore landed in `mVersion` (truncated) and `mId` was left 0. Reading the engine's own `CommandItem::createInstance` settled the rest: it only treats `mId` as the item's own id when `mVersion` is **non-zero** - with 0 it converts the value as a *legacy* id through a different table, which is why an entry for `minecraft:stick` resolved to `minecraft:oak_sign` while our item resolved to nothing. The entry is now `{1, true, id}`. The registry suite builds that value and asserts `createInstance` returns the item (`give.*`)
- Note for whoever tests this next: the item must be reached by a **real** player - a virtual one gives `No targets matched selector`; and an item whose identifier contains a dot (`modapi_test:item.test_block`) is rejected by the command parser outright, so a block item registered under that name cannot be given at all

- The definition packet a client receives was never filled in for custom items. `ModAPI` hooked
  `ItemRegistryPacket::write`, but the old build shows the packet is serialized through
  `writeWithSerializationMode` reaching `serialize<ItemRegistryPacketPayload>::write` directly - `write` is a
  different virtual, so the hook never ran. It is now on the payload constructor, which is where `mItems` is
  produced and which every sending path goes through. Reading `components` out of an entry that has none also
  threw `bad variant access` and aborted the whole merge, so the node is only read after a check
- `ICustomItem` stopped setting `mItemParseVersion = ItemVersion::DataDriven`: it was commented out during an
  experiment and left that way. That flag is what gives an item the definition machinery a client needs, and
  custom items came up blank from then on (`Item <name> requires either an icon atlas or icon texture`)
- Re-registering an item name left the replaced item in `ItemRegistry::mItemRegistry`, and
  `CreativeItemRegistry` appended a creative entry per registration, so items showed up twice in a client's
  inventory and the leftover reported default fields. Registration now drops the replaced entry, and
  `registerCreativeItem` replaces the entry for an item instead of appending another
- `/give <custom item>` silently did nothing: the item enum entry was written as `CommandItem{id, true, false}`,
  but the type is a union of `{ short mVersion; bool mOverrideAux; int mId; }`. `CommandItem::createInstance`
  only reads `mId` as the item's own id when `mVersion` is non-zero - with 0 it converts the value as a *legacy*
  id through a different table
- A block item's id has to be the block's id folded into a short (`id <= 0xFF ? id : 255 - id`), which is what
  `ItemRegistryRef::registerBlockItem<BlockItem>` hands to `BlockItem`'s constructor. Assigning an id afterwards
  left the item's own state disagreeing with `mId`, which showed up as a creative entry nothing could be done
  with
- A block whose type a mod registers in C++ is not listed by the engine at all: the engine creates items for the
  blocks it parses out of documents, so that item is queued for the creative pass instead. The queue cannot
  duplicate anything now that `registerCreativeItem` replaces

- A block item drawn as the block itself (rather than as a flat picture) needs the `minecraft:block_placer`
  component in its definition, with the block it stands for. Two details decide whether a client acts on it, and
  both come out of `PlanterItemComponent::buildNetworkTag` in the old build, which writes `canUseBlockAsIcon` from
  the component's byte +64 and `replaceBlockItem` from +65: the keys are **camelCase** in the network form (a
  *document* uses `replace_block_item`, bound by `bindType`), and `canUseBlockAsIcon` has no document key at all -
  the definition sent to a client is the only place it can be set. A `minecraft:icon` must be left out at the same
  time: `ComponentItem::getIconInfo` reads the icon component first and only falls back to the item's block when
  there is none, so writing an icon pins the item to a texture

- A block registered from C++ now reaches clients without any document. The engine builds the list a client is
  told about (`StartGamePacketPayload::mBlockProperties`) out of the block definition group, and a block whose type
  ModAPI registered itself is not in that group - so the packet's write path adds the entry it is missing. The entry
  is field for field the one the engine writes itself (read out of a live one as SNBT), which is why it works: the
  same `components`/`minecraft:material_instances`, `menu_category`, `molangVersion` and `vanilla_block_data`, with
  the block's own id. `ServerBlockProperty`'s default constructor is declared in the header but not exported, so its
  definition is written here.
- The document path is gone with it: `registerBlockFromMemoryJson`, `registerBlockFromJsonFile`, `deferBlockDocument`
  and the runtime pack they installed were removed, along with the suppression of the engine's block type and the
  bookkeeping that went with it. A block is a `::BlockType` subclass registered under an identifier, and the texture
  a client draws it with is named with `BlockRegistry::setBlockTexture` - the texture file and the resource pack
  mappings (`blocks.json`, `terrain_texture.json`) are the mod's own, exactly as they are for an addon block.
- `ICustomBlock` is gone as well: it only forwarded a constructor to `::BlockType`, so a mod derives from
  `::BlockType` directly. `ICustomFeature` moved to `include/modapi/worldgen/base/` and
  `src/modapi/worldgen/base/`, which is where every other registry keeps its interfaces.

- The definition-level part of a block entry comes out of the registration's own `BlockDescription` instead of
  defaults: `menu_category.category` through the engine's exported
  `SharedTypes::v1_21_110::ItemCategory::stringFromCreativeItemCategory`, `group` and `is_hidden_in_commands` from
  `BlockMenuCategory`, and `vanilla_block_data.block_id` / `material` from `VanillaBlockData` (the material name
  through `BlockEnum::MaterialTypeToString`). `BlockPermutationDescription`s are published as `permutations`, each
  with its `condition` (`ExpressionNode::getExpressionString`), its components and its tags - the same three nodes the
  engine writes.
- A block is registered with what to tell a client about it, in one call and in the engine's own types:
  `registerBlock<MyBlock>("mymod:my_block", { .mArguments = {...}, .mProperty = {...} })`, with `BlockProperty` made
  of `BlockComponentGroupDescription` / `BlockDescription` / `BlockPermutationDescription`. The same property drives
  both sides: its components are injected into the running block type through `initializeComponentFromCode` - the
  route vanilla uses for its own blocks - and serialised for a client through `isNetworkComponent` + `getName` +
  `buildNetworkTag`, which are the calls `generateServerBlockProperties` makes. The entry produced this way matches
  the engine's own field for field (measured against the entry the engine writes for an addon block: identical apart
  from each block's own texture name and id). No `setBlockTexture`/`setBlockProperty`: a property is handed over with
  the registration.
- The engine declares default constructors and copy operations for those definition types and does not export them,
  so `include/modapi/block/base/EngineDefaults.h` supplies the definitions inline - default constructors only, and
  copy/assignment only where the linker proved them missing. A type holding a `CompoundTag`
  (`ServerBlockProperty`) keeps the engine's exported copy: a memberwise `= default` there copies the tag shallowly
  and two objects then free the same storage, which crashed the server at `StartGamePacket` time.

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
