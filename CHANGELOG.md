# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Since `26.10.0` the version number tracks the supported LeviLamina release: `26.40.0` is the build for LeviLamina 26.40.x.

<!-- The release workflow extracts the release notes from the *first* `## [x.y.z]` section of this
     file - it does not look the tag up by version. The section of the version being released
     therefore always has to be the topmost one, directly below `[Unreleased]`. -->

## [Unreleased]

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
