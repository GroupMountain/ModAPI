# Repository Guidelines

## Project Overview

ModAPI is a Windows x64 C++20 DLL for LeviLamina/BDS servers. It exposes a uniform registration API for custom items, blocks, recipes, loot tables, game rules, biomes, worldgen features/rules/surface builders, and creative groups.

Design priorities:

- Engine-compatible injection over abstraction.
- Breaking public API changes are allowed; do not add compatibility aliases.
- Public headers contain usage and contracts only. Design rationale and engine research belong in `docs/internal/`.

## Architecture & Data Flow

1. A user describes an entry with `Registry::builder<T>("id")...build()`.
2. `.build()` produces `ClassRegistration<Entry, Args...>` or `DataRegistration<Entry>` (`include/modapi/Registration.h`).
3. `DeferredRegister<Registry, Entry>` stores that registration object and listens for `Registry::EventType`.
4. A vanilla-pass hook calls `_bindRegistry` / `_bindRules`; the registry publishes its ready event.
5. `DeferredRegister` replays the registration into `Registry::registerEntry(...)`, which builds/injects the entry and returns `optional_ref<Product>`.

Every registry satisfies `modapi::Registry<T>` in `include/modapi/Registry.h`:

- `EventType` (an `ll::event::Event` subclass)
- `Product` and `ProductRef = optional_ref<Product>`
- `isReady()`, `getInstance()`, `ensureEventRegistered()`

Registries are singleton, hook-owning objects. `ensureEventRegistered()` is idempotent and must run from `ModAPI::load()` or `DeferredRegister`, not a static initializer.

### Identifier placement

This is registry-specific and easy to break:

- `ItemRegistry`: constructor receives the identifier first: `Entry(identifier, args...)`.
- `BlockRegistry`: identifier and allocated block id go to `BlockTypeRegistry::registerBlock<Entry>`; `args...` follow.
- `RecipeRegistry`: identifier is prepended only for concrete `Custom*Recipe`; user `ICustom*Recipe` subclasses are constructed from `args...` only.
- `FeatureRegistry` / `FeatureRuleRegistry` / `SurfaceBuilderRegistry`: identifier is the registry key, constructor gets `args...`.
- Data registries: `T::makeDataEntry(identifier, property)` assigns the identifier.

### Registration shapes

- Class entries: `ClassRegistrationBuilder<T, Entry>`; chain `.arguments(...)`, then `.build()`.
- Data entries: `DataRegistrationBuilder<T>`; `.property(...)`, then `.build()`.
- Data registries with fluent options use bespoke builders (`LootTableRegistrationBuilder`, `GameRuleRegistrationBuilder`, `CreativeGroupRegistrationBuilder`, `RecipeBuilder`).
- `DeferredRegister` has no `operator*` / `operator->`; use `.get()`.
- Registries are singleton, hook-owning `PImpl` objects; most are deliberately leaked so hooks survive DLL teardown. Put engine pointers, hook registrars and maps in `Impl`.
- Code uses `namespace modapi::inline <module>`; `modapi::ItemRegistry` and `modapi::item::ItemRegistry` are the same symbol.
- When adding a registry, also instantiate its deferred path in `src/modapi/core/RegistryTemplateCheck.cpp`.

## Key Directories

| Path | Purpose |
|---|---|
| `include/modapi/` | Published API; only these headers ship in `bin/include`. |
| `include/mc/` | Patched SDK shadow headers. Do not hand-edit generated files. |
| `src/modapi/` | Registry implementations, hooks, core utilities, private headers. |
| `src/mc/` | Definitions for SDK-declared but unexported engine symbols. |
| `src-test/` | LeviLamina test mod; opt-in `test` target. |
| `scripts/` | Version/manifest generation, header patching, include correction, formatting. |
| `docs/internal/` | Design decisions, engine/IDA notes, history, verification details. |
| `bin/` | Build/deployment output; wiped by a ModAPI build. |
| `build/config/` | Generated `modapi/Version.h`. |

## Development Commands

```bash
# Configure (Windows x64, clang-cl)
xmake f -m release -a x64 -p windows -y

# Library
xmake build ModAPI

# Test mod (opt-in; target default=false)
xmake build test
```

Build ModAPI **before** test: the ModAPI `after_build` step wipes and recreates `bin/`; building it after `test` deletes the test output.

Outputs:

```text
bin/dll/ModAPI/ModAPI.dll + manifest.json
bin/test/dll/test/test.dll + manifest.json
bin/include/
bin/pdb/
```

Deploy a test server as:

```text
<server>/
  bedrock_server_mod.exe
  plugins/
    ModAPI/{ModAPI.dll,manifest.json}
    test/{test.dll,manifest.json}
```

Start `bedrock_server_mod.exe`; reports are written to `plugins/test/*.txt`. In-game reruns: `modapitest registry|blocks|biome|loottable|blockhelper`.

## Code Conventions & Common Patterns

- C++20, clang-cl, Windows x64. `/GR-` is set: **no RTTI** (`dynamic_cast`, `typeid`).
- Treat exceptions as disabled across the API boundary: wrap engine work, catch `std::exception`/`...`, log via `core::getLogger()`, and return an empty `ProductRef`/`false`. The library target sets `set_exceptions("none")` while also passing `/EHsc`; do not rely on throwing across modules.
- Public export macros: `MOD_API`, `MOD_NDAPI`, `MOD_CAPI` from `include/modapi/Macros.h`.
- Naming: members use `m` prefix; types use `PascalCase`; registries expose `builder`, `registerEntry`, `isEntry`.
- Builder methods return `*this` for chaining; `.build()` returns a copyable registration object.
- Item/block are templates: `ICustomItem<T> : T`, `ICustomBlock<T> : T`; users subclass these, not side property objects.
- `ICustomItem::initComponentItem(ctx)` creates the inner `ComponentItem`, inserts components, sets `mIcon` for `minecraft:icon`, and calls each component's `_initializeComponent`. Block items get no icon; `PlanterItemComponent::_initializeComponent` sets the block rendering pointer. Do not add a manual legacy `item_properties` writer: `ComponentItem::buildNetworkTag()` writes the engine schema once `mIcon` is set.
- Engine symbols declared but not exported go in `src/mc/*.cpp` or `src/modapi/**/*.cpp` as `Type::Type() = default;` etc. Never put these definitions in public headers.
- Public headers: usage/contract only. Put design decisions in `docs/internal/design-decisions.md`, engine/IDA details in `docs/internal/engine-notes.md`, and version history in `docs/internal/history.md`; do not put version-specific notes in source comments.
- `README.md` is Chinese usage/contract documentation. `CHANGELOG.md` is English, Keep a Changelog style, API-level entries under `[Unreleased]`.
- Do not add `TODO`/`FIXME` placeholders or compatibility shims.
- `ModAPI::load()` does not call `BiomeRegistry::ensureEventRegistered()`; a mod using biomes must construct a `DeferredRegister<BiomeRegistry, ...>` or call it explicitly.

### Common pitfalls

- Do not edit `include/mc/world/item/HumanoidArmorItem.h`; `scripts/header_file_patch.py` regenerates it on every project load.
- `before_build` runs `include_correction.py` and `format_all.py`; expect include style and formatting changes.
- `format_all.py` only formats `src/` and `include/`, not `src-test/`.
- Do not run `xmake test`; there is no xmake test target. Tests are a native mod run on a live server.
- `ClassRegistration` must stay copyable for `DeferredRegister` replay.
- `DeferredRegister::get()` is empty until a ready event runs; furnace/brewing/group registrations intentionally have no product.

## Important Files

- `src/modapi/core/ModAPI.cpp` — mod entry point, load/disable, registry event setup.
- `include/modapi/Registry.h` — `Registry` / `RegistryEntry` concepts.
- `include/modapi/Registration.h` — `ClassRegistration`, `DataRegistration`, builders.
- `include/modapi/DeferredRegister.h` — replay/listener lifecycle.
- `include/modapi/item/base/ICustomItem.h` — item template, components, `initComponentItem`.
- `include/modapi/block/base/ICustomBlock.h` — block template and `BlockProperty`.
- `include/modapi/recipe/RecipeRegistry.h` + `include/modapi/recipe/types/` — recipe builders and concrete classes.
- `include/modapi/worldgen/BiomeRegistry.h`, `FeatureRegistry.h`, `FeatureRuleRegistry.h`, `SurfaceBuilderRegistry.h`.
- `include/modapi/loot_table/LootTableRegistry.h`, `gamerule/GameRuleRegistry.h`, `item/CreativeItemRegistry.h`.
- `xmake.lua`, `src-test/xmake.lua`, `rules/linkrule.lua`.
- `src-test/Test.hpp`, `TestCore.cpp`, `TestReport.hpp`, `Global.h` — test harness.
- `docs/internal/design-decisions.md`, `engine-notes.md`, `history.md`, `verification.md`.

## Runtime/Tooling Preferences

- Required: Windows x64, xmake, clang-cl/LLVM 23, Python (`python`, not `python3`), `clang-format` on `PATH`.
- Packages: `levilamina 26.51.*` (server), `levibuildscript 0.6.*`, `minizip-ng 4.0.9`.
- xmake config is release/x64/windows; non-default build directories are not supported by deployment/include hooks.
- `set_runtimes("MD")` unless explicitly overridden.
- Do not rely on RTTI; prefer virtual dispatch, tags/markers, and `static_cast`.
- Build hooks run Python and clang-format automatically; do not manually reimplement their output.

## Testing & QA

- Test mod: `src-test/`, built with `xmake build test` (target is `set_default(false)`).
- Harness: `REGISTER_ON_LOAD_TEST` queues load-time setup; `REGISTER_TEST` attaches a `ServerStartedEvent` listener. All listeners must be removable (`test::listen`) before DLL unload.
- Reports: `test::Report` writes `plugins/test/*.txt`; final line is `TAG RESULT passed=N failed=N skipped=N`.
- Expected error lines (negative cases) are documented in tests; do not treat them as failures.
- `skip` is for environment-dependent cases (e.g. no newly generated chunks); a broken contract should fail.

| Tag | Report | Coverage |
|---|---|---|
| `[REGISTRY]` | `registry-test-report.txt` | Deferred registrations, readiness, ready events, concepts, item/creative/recipe/gamerule/feature APIs, JSON recipes, listener lifecycle |
| `[BLOCKS]` | `blocks-test-report.txt` | Block registration/properties, client definitions, block item link, item components/icons, resource packs |
| `[BIOME]` | `biome-test-report.txt` | Biome registration/lattice, surface builder writes/readback, client pack |
| `[LOOTTABLE]` | `loottable-test-report.txt` | JSON/DOM/builder tables, engine lookup, deferred registration, unregister |
| `[BLOCKHELPER]` | `blockhelper-test-report.txt` | Worldgen block handle read/write/layers/bounds |

Test environment assumptions: overworld dimension id 0, loaded chunks near spawn, simulated player anchors, height range from `BlockSource`. Existing worlds are fine; biome surface readback only observes newly generated chunks and skips otherwise.

Verification checklist:

```bash
xmake build ModAPI
xmake build test
# deploy both plugin dirs, start server, expect failed=0 in every report
```
