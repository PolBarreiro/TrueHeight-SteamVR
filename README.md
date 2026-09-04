<div align="center">

<img src="docs/images/trueheight-logo-v2.png" width="140" alt="TrueHeight">

# TrueHeight

**Automatic height stabilization for SteamVR.**

TrueHeight adjusts SteamVR's temporary standing-space offset so your headset
stays near a chosen height while you sit, recline, or change posture.

[![Release](https://img.shields.io/github/v/release/PolBarreiro/TrueHeight-SteamVR?label=release)](https://github.com/PolBarreiro/TrueHeight-SteamVR/releases/latest)
[![License: CC BY-NC-ND 4.0](https://img.shields.io/badge/license-CC%20BY--NC--ND%204.0-lightgrey)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows%20x64%20%7C%20SteamVR-blue)](#requirements)

</div>

## What it does

TrueHeight runs as a small Windows tray utility. It reads the headset pose from
SteamVR and applies a temporary standing-space preview offset. It does not
inject code into games or permanently commit changes to Room Setup.

It is designed to be game-independent and useful for seated players, wheelchair
users, and anyone who wants a stable in-game standing height. Compatibility can
still vary with the headset runtime and with software that also moves the play
space.

## Requirements

| | |
|---|---|
| OS | Windows 10 or 11, 64-bit |
| Runtime | SteamVR and a SteamVR-compatible headset |
| Install size | Under 10 MB |
| Administrator rights | Not required |

## Quick start

1. Start SteamVR and make sure headset tracking is active.
2. Start `TrueHeight.exe`. The settings window can be closed; TrueHeight keeps
   running from the notification area.
3. Set **Target height** to your normal standing eye height and choose
   **Apply & Save**.
4. Put on the headset and enable the height lock.
5. To stop and restore the calibrated floor, press <kbd>F9</kbd> or choose
   **Restore real floor**. Restore also pauses the lock so it stays restored.

TrueHeight never starts SteamVR itself. When installed from Steam, launch
TrueHeight from your Steam Library before starting the VR title.

## Hotkeys

The hotkeys are global and work while a game has focus.

| Key | Action |
|-----|--------|
| <kbd>F7</kbd> | Reload the saved configuration |
| <kbd>F8</kbd> | Pause or resume the height lock |
| <kbd>F9</kbd> | Restore the calibrated floor and pause the lock |

The large button at the bottom of the settings window performs the same
start/stop action as <kbd>F8</kbd>. The lock starts enabled by default.

## Configuration

The settings window covers the common options. The full configuration is saved
per Windows user at:

```text
%APPDATA%\TrueHeight\config.ini
```

Diagnostic events are written beside it in `TrueHeight.log`; the log contains
runtime status and settings values, not headset serial numbers or telemetry.

On first launch, TrueHeight uses the `config.ini` shipped beside the executable
as its template. A Steam update therefore cannot overwrite personal settings.

| Key | Default | Meaning |
|-----|---------|---------|
| `target_height_cm` | `190` | Desired headset height above the play-area floor. Set this to your own standing eye height. |
| `smoothing_time_ms` | `800` | How gently the offset catches up. Lower values feel tighter. |
| `deadzone_cm` | `8` | Height error ignored before correction begins. |
| `max_speed_m_s` | `2.0` | Comfort limit for vertical correction speed. |
| `update_hz` | `60` | Control-loop rate. |
| `max_offset_m` | `3.0` | Safety clamp on total temporary offset. |
| `only_when_worn` | `true` | Pause adjustment when a supported proximity sensor reports that the headset is off. |
| `start_enabled` | `true` | Enable the lock on launch. |
| `invert` | `false` | Emergency compatibility switch if a runtime reports the vertical axis in the opposite direction. |

The legacy `restore_on_exit` key is accepted for compatibility, but current
versions always attempt to restore the calibrated floor on a clean exit.

## Safety and compatibility

- TrueHeight uses SteamVR's temporary working-set preview; it does not commit a
  new permanent chaperone calibration.
- Do not use OVR Advanced Settings Space Offset, Motion, Height Toggle, or
  another play-space mover at the same time. The tools would fight over the
  same SteamVR state.
- Height stabilization intentionally cancels physical vertical movement. Use
  the game's crouch input to make the player crouch. You may also crouch
  physically if that feels natural, but the button still has to be pressed
  because TrueHeight is maintaining the headset height.
- A process or PC crash can prevent cleanup code from running. Restarting
  TrueHeight reverts any leftover working preview before beginning; restarting
  SteamVR also clears the session.
- Test the direction while seated and ready to press <kbd>F9</kbd>. If the floor
  visibly runs away rather than converging, press <kbd>F9</kbd>, enable
  `invert=true`, and report the headset/runtime combination.

## Building and testing

The supported release toolchain is Visual Studio Build Tools 2022 and CMake:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Valve's OpenVR header, import library, runtime DLL, and BSD-3-Clause notice are
included in the repository. Release builds link the Microsoft C++ runtime
statically; `openvr_api.dll` must remain beside `TrueHeight.exe`.

Run `scripts/prepare-steam-build.ps1` to compile, test, refresh the Steam depot
folder, and write SHA-256 checksums. Use `TrueHeight.exe --no-vr` for a safe UI
smoke test that does not connect to SteamVR. Automated smoke tests may set the
`TRUEHEIGHT_CONFIG_DIR` environment variable to keep test settings isolated.

## Updates and forks

Official updates are published through Steam and
[GitHub Releases](https://github.com/PolBarreiro/TrueHeight-SteamVR/releases).
Users who clone the repository can receive new official commits with
`git pull`. The CC BY-NC-ND license permits private modifications but does not
permit publishing modified builds or derivative forks.

## License

The public GitHub/Nexus edition is licensed under
[CC BY-NC-ND 4.0](LICENSE). The copyright holder may distribute commercial
builds, including the Steam edition, under separate terms.

Bundled OpenVR files are Copyright (c) 2015 Valve Corporation and licensed
under BSD 3-Clause; see [LICENSE-OpenVR.txt](LICENSE-OpenVR.txt) and
[NOTICE.md](NOTICE.md).

## Credits

Created by [Pol Barreiro Font](https://github.com/PolBarreiro). Built with
Valve's [OpenVR SDK](https://github.com/ValveSoftware/openvr).
