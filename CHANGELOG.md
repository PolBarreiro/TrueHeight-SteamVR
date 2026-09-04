# Changelog

All notable changes to TrueHeight are documented here.
Format based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/);
versioning follows [Semantic Versioning](https://semver.org/).

## [Unreleased]

### Added
- Native Windows settings window and notification-area controls.
- Unit tests for deadzone, smoothing, speed limiting, and offset clamping.
- `--no-vr` mode for safe UI smoke tests.
- Reproducible MSVC release and Steam-depot preparation workflow.
- Original project-owned TrueHeight brand mark and Windows icon.

### Changed
- Configuration is stored per user under `%APPDATA%\TrueHeight` and is seeded
  from the default file beside the executable on first launch.
- SteamVR changes use only the temporary working-set preview and are never
  committed to the user's live Room Setup calibration.
- F9 and **Restore real floor** now pause the lock after restoring.
- Clean exit always attempts to restore the calibrated floor.
- SteamVR auto-start registration was removed because that API only supports
  dashboard overlays; `--unregister` remains for legacy cleanup.
- Shared worker/UI state now uses atomics instead of data-racy `volatile` values.

### Fixed
- Headset-presence detection now accepts SteamVR's interaction-timeout state and
  does not block headsets without a proximity sensor.
- Standing-origin resets and Room Setup changes rebase the temporary offset.
- Version metadata and application version are consistently `1.0.0`.

## [0.1.0] — 2026-07-25

First public release.

### Added
- Automatic SteamVR floor-height lock: continuously offsets the vertical origin
  of the standing tracking universe so the HMD stays at a fixed height above the
  play-area floor.
- Works at the chaperone level, so it applies to **every** SteamVR title and any
  headset that connects to SteamVR (Quest via Link / Virtual Desktop / ALVR,
  Index, Vive, Pico, ...). No game mods, no injected DLLs.
- Console status display: live height, applied offset and lock state.
- `config.ini` with target height, smoothing time, deadzone, max correction
  speed, `only_when_worn`, `restore_on_exit` and `invert`.
- Global hotkeys — **F7** reload config, **F8** pause/resume, **F9** restore the
  real calibrated floor.
- `--register` / `--unregister` to auto-start with SteamVR through
  `trueheight.vrmanifest`.
- Floor is restored automatically on a clean exit.

### Known issues
- The chaperone grid can flash briefly during large corrections (e.g. the
  initial lock-on while seated).
- The correction direction was derived analytically rather than measured on
  hardware. If the floor runs away instead of locking, set `invert=true`.

[0.1.0]: https://github.com/PolBarreiro/TrueHeight-SteamVR/releases/tag/v0.1.0
