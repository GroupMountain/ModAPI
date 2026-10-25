# Design decisions

These are maintainer notes. Public headers keep usage and contracts only; this file keeps the reasoning.

## Item and block are templates

`ICustomItem<T>` and `ICustomBlock<T>` inherit the engine base directly. The user's class is both the
behaviour and the builder: components/description live on the object, so there is no `ItemProperty` /
`BlockProperty` side channel and no second registration object.

`ICustomBlock` owns `BlockProperty` privately because the registry needs the block's own description
when it publishes the client entry. `BlockRegistry` is a friend; users go through the fluent methods.

## Data entries do not inherit

Loot tables, game rules, biomes and creative groups have no behaviour to override. They use
`DataRegistration<Entry>` and `DataRegistrationBuilder`; making users subclass them only added a
virtual `_init()` that did nothing useful.

## Recipe is hybrid

All `ICustom*Recipe` interfaces stay public. The concrete `Custom*Recipe` classes are public and
directly constructible, so a data recipe needs no subclass. A user who needs behaviour subclasses
any `ICustom*Recipe` and uses the generic `.arguments(...)` builder.

Multi recipes are not fixed to `::MultiRecipe`. `CustomShapedMultiRecipe`/`CustomShapelessMultiRecipe`
keep the public interface, and `_init()` builds an internal engine recipe from `::ShapedRecipe` /
`::ShapelessRecipe` (with the callback), because those are the bases that carry the shape data.

## No legacy item NBT writer

`ComponentItem::buildNetworkTag()` already writes `item_properties` and the component map. The old
`writeLegacyItemProperties` duplicated those fields and gave block items an icon they must not have.
It was removed.

The one non-obvious engine dependency is the icon pointer: `ComponentItem::buildNetworkTag()` writes
`item_properties.minecraft:icon` only when `ComponentItem::mIcon` is non-null. `IconItemComponent`'s
`_initializeComponent` only sets the icon name, so `initComponentItem` assigns `mIcon` explicitly.
`PlanterItemComponent::_initializeComponent` sets `mBlockTypeForRendering`, so block items render
from their block without an icon component.

## Registration shape

Every registry exposes `builder(...)...build()`. Class entries use `ClassRegistration<Entry, Args...>`;
data entries use `DataRegistration<Entry>`. `DeferredRegister` stores the registration object and
replays it on each ready event; it does not expose `operator*`/`operator->`.

## Documentation split

- Public headers: usage and contract.
- `README.md` / `CHANGELOG.md`: user-facing usage and API-level changes.
- `docs/internal/`: engine observations, IDA notes, history, verification details.

## API compatibility

Breaking public API changes are allowed. Do not add compatibility aliases, deprecated re-exports or
transition shims; update call sites and delete the old path.
