TrueHeight 1.0.0
================

Automatic height stabilization for SteamVR.

1. Start SteamVR and confirm headset tracking is active.
2. Launch TrueHeight from your Steam Library.
3. Set your standing eye height, then choose Apply & Save.
4. Press F9 at any time to restore the calibrated floor and pause the lock.
5. Use the large START/STOP button at the bottom of the window, or press F8,
   to resume or pause the lock. The lock starts enabled by default.

Personal settings:
  %APPDATA%\TrueHeight\config.ini
Diagnostic log:
  %APPDATA%\TrueHeight\TrueHeight.log

TrueHeight applies only a temporary SteamVR working-set preview. It does not
commit a permanent Room Setup calibration. Do not run another play-space
offset/motion tool at the same time.

Support and source:
  https://github.com/PolBarreiro/TrueHeight-SteamVR

Copyright (c) 2026 Pol Barreiro Font. All rights reserved.
Includes Valve OpenVR components under BSD 3-Clause; see LICENSE-OpenVR.txt.
