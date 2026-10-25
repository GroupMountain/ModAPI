# Verification

The test mod lives in `src-test/`; build it with `xmake build test`.

Runtime checks used during development:

- Deploy `bin/dll/ModAPI` and `bin/test/dll/test` into a server's `plugins/` directory.
- Start `bedrock_server_mod.exe`; the suites run on `ServerStartedEvent`.
- Read `plugins/test/registry-test-report.txt`, `blocks-test-report.txt`, `biome-test-report.txt` and
  `loottable-test-report.txt`, or grep the server log for `[REGISTRY]`, `[BLOCKS]`, `[BIOME]`, `[LOOTTABLE]`.

Item checks:

- `/give @s modapi_test:registry_test_item` shows the icon, name, stack size and durability from components.
- The test asserts `getComponentItem() != nullptr`, the description category matches the item's creative
  fields, and `tags()` matches `TagsItemComponent`.

Block checks:

- The test registers a C++ block and a C++-only block, publishes their client properties, and asserts the
  block item's `mBlockType` points at the custom block.

Biome checks:

- `/locate biome` for three different coordinates, F3, and the `[BIOME]` suite.
