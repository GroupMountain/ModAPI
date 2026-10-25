# Engine notes

These are implementation observations, not public API documentation.

## Items

- `::Item` keeps both legacy fields (`mCreativeCategory`, `mCreativeGroup`, `mIsHiddenInCommands`) and the
  component-based definition. `ICustomItem<T>` writes the legacy fields from `ItemDescription` after the
  registry assigns the id, and publishes the component map through `::ComponentItem`.
- `ComponentItem::buildNetworkTag()` may return either a flat component map or a map with a `components`
  child. `ItemRegistry::prepareItemDefinitions` accepts both.
- `ItemDescription`, `SharedTypes::v1_26_0::PlanterItemComponent` and `Bedrock::PubSub::Subscription`
  constructors are declared by the SDK but not exported. They are defined in `src/mc/ItemDescription.cpp`;
  templates only call the exported factories.

## Blocks

- `BlockTypeRegistry::registerBlock<Entry>` constructs the entry with `(identifier, id, args...)` and takes
  ownership. The registry then reads `static_cast<Entry&>(block).getProperty()`.
- `BlockDescription`, `BlockComponentGroupDescription`, `BlockPermutationDescription`, `ServerBlockProperty`
  and `ExpressionNode` default constructors are not exported; their definitions live in
  `src/modapi/block/BlockRegistry.cpp`.
- Client block properties are rebuilt from `BlockProperty` in `_publishClientProperties`, because a block
  registered from C++ has no definition-group entry.

## Recipes

- 26.51 splits the JSON recipe loader into `RecipeType` lookup plus `Recipes::_loadRecipe(..., Buffers&)`.
- Furnace and brewing recipes do not produce a `::Recipe`; their registrations return an empty product.
- Multi recipes are real engine recipe subclasses; the concrete `Custom*MultiRecipe` classes keep the public
  interface and build the engine object through `_init()`.

## Worldgen

- Surface builder writes go through the chunk staging volume (`PerColumnParameters::mBlocks`), y-first:
  `index = localY + mHeight * (localZ + localX * mDepth)`.
- Biome component keys are process-specific and not exported; `BiomeRegistry::dumpComponentKeys()` prints them.

## Item icon (1.26.30 client, IDA)

`ComponentItem::initializeFromNetwork` reads the icon from the `item_properties` map (`item_properties.minecraft:icon`),
constructs the `IconItemComponent` at `ComponentItem+536`, and calls its `_initializeComponent`. A top-level
`components.minecraft:icon` alone does not populate that pointer in this build.

Therefore `ItemRegistry::prepareItemDefinitions` copies the merged icon payload into `item_properties` for
non-block items only. A block item must not get an icon: `PlanterItemComponent::_initializeComponent` sets
`ComponentItem::mBlockTypeForRendering` from `block_placer`, and `ComponentItem::getIconInfo` then draws the block.

## Item factories

`ItemDescription`, `SharedTypes::v1_26_0::PlanterItemComponent` and `Bedrock::PubSub::Subscription` constructors
are declared by the SDK but not exported. They are defined in `src/mc/ItemDescription.cpp`; the public template
calls the exported factories instead of naming those constructors in every consumer TU.
