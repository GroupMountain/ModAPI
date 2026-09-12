# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed

- Adapted LeviLamina 26.40.0 and BDS 26.40.8
- Replaced `CraftingDataPacket::prepareFromRecipes` with `CraftingDataPacketPayload::fromRecipes`, which was renamed in 26.40
- Rebuilt the smithing recipes by hand: 26.40 only declares the `SmithingTransformRecipe` / `SmithingTrimRecipe` constructors for the client, so the recipe fields are filled in directly
- Followed the `ResourcePackStack::deserialize` signature change (stream to `std::string_view`) and the `PackManifest` header move
- Dropped the now redundant local definitions of `ItemInstance::ItemInstance` and `CreativeGroupInfo::~CreativeGroupInfo`

## [0.3.1] - 2026-1-31

## Changed

- Adapted LeviLamina 1.9.2 and BDS 1.21.132 @killcerr @n15421
- Clean tmpfix for initServer and initclient @n15421

## [0.3.1] - 2025-11-28

## Changed

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

[Unreleased]: http://github.com/GroupMountain/ModAPI-Release/compare/v0.3.1...HEAD
[0.4.0]: http://github.com/GroupMountain/ModAPI-Release/compare/v0.3.1...v0.4.0
[0.3.1]: http://github.com/GroupMountain/ModAPI-Release/compare/v0.3.0...v0.3.1
[0.3.0]: http://github.com/GroupMountain/ModAPI-Release/compare/v0.2.1...v0.3.0
[0.2.1]: http://github.com/GroupMountain/ModAPI-Release/compare/v0.2.0...v0.2.1
[0.2.0]: http://github.com/GroupMountain/ModAPI-Release/compare/v0.1.1...v0.2.0
[0.1.1]: http://github.com/GroupMountain/ModAPI-Release/compare/v0.1.0...v0.1.1
[0.1.0]: http://github.com/GroupMountain/ModAPI-Release/releases/tag/v0.1.0
