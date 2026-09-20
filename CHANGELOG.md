# Changelog

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased]

### Added

- Add an optional MCM addon
- Add a setting for debug logging

### Changed

- Reduce per-frame work when centering item previews
- **Breaking change:** Move user settings to
  `MCM/Settings/ItemPreviewAutoSpinning.ini`

### Fixed

- Support Skyrim Steam AE 1.7.104
- Correct item preview rotation and spin speed on VR
- Reject non-finite INI values and report settings file errors

## [0.1.9] - 2026-06-05

### Fixed

- Keep the plugin loading when preview-centering hooks are unavailable
- Exclude spell and enchantment previews from inventory preview spinning in
  crafting menus

## [0.1.8] - 2026-06-04

### Fixed

- Keep auto-spinning item previews centered when mesh effect geometry inflates
  the preview bounds
- Keep manually rotated item previews centered when mesh effect geometry
  inflates the preview bounds

## [0.1.7] - 2026-06-01

### Fixed

- Exclude magic effect previews

## [0.1.6] - 2026-05-30

### Fixed

- Spin item previews outside the inventory menu

## [0.1.5] - 2026-05-30

### Fixed

- Avoid release spin after slow manual repositioning

## [0.1.4] - 2026-05-27

### Added

- Allow negative auto-spin speed to reverse direction

## [0.1.3] - 2026-05-27

### Added

- Continue manual item rotation briefly after releasing a drag

## [0.1.2] - 2026-05-27

### Added

- Add configurable rotation speed and resume delay

## [0.1.1] - 2026-05-27

### Fixed

- Package release archives with MO2-compatible ZIP metadata

## [0.1.0] - 2026-05-27

### Added

- Initial release
