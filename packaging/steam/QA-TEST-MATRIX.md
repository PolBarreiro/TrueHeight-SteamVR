# TrueHeight 1.0.0 release QA

Tester:
Headset:
Connection/runtime (native SteamVR, Link, Virtual Desktop, ALVR, other):
SteamVR version:
Windows version:
Build SHA-256:
Date:

Use the exact files from packaging/steam/content. Keep one hand ready to press
F9 and begin with a clear seated play area.

| ID | Test | Expected result | Pass / notes |
|---|---|---|---|
| D1 | Launch TrueHeight.exe --no-vr with SteamVR closed. | Settings window opens, says diagnostic mode, and does not start SteamVR. | |
| D2 | Close the window with X. | Window hides; tray icon remains and app continues. | |
| D3 | Open Settings from the tray, then choose Quit. | Window returns; Quit ends the process and removes the tray icon. | |
| C1 | Delete only the test user's %APPDATA%\TrueHeight\config.ini, then launch. | A writable config is created from the shipped defaults. | |
| C2 | Change height and smoothing, choose Apply & Save, quit, and relaunch. | Values persist after restart. | |
| C3 | Replace the install-folder config with different values and relaunch. | Existing AppData values remain unchanged. | |
| V1 | Start SteamVR with tracking active, then launch TrueHeight. | Status changes to connected; SteamVR was not restarted. | |
| V2 | While seated, set the target 20 cm above the current reported height. | Offset converges smoothly in the correct direction without runaway movement. | |
| V3 | Press F8 while locked, wait, then press F8 again. | First press freezes the offset; second resumes correction. | |
| V4 | Press F9 while an offset is active. | Calibrated floor returns, offset reads 0, and lock remains paused. | |
| V5 | Re-enable the lock, then choose Restore real floor in the window. | Same durable restore and pause behavior as F9. | |
| V6 | With lock active, remove a headset that has a proximity sensor. | State becomes HEADSET OFF and the offset stops changing. | |
| V7 | Briefly interrupt tracking. | State becomes NO TRACKING and no offset change occurs until valid tracking returns. | |
| V8 | Run SteamVR recenter/standing-origin reset. | TrueHeight rebases without runaway motion or permanent calibration change. | |
| V9 | Quit TrueHeight from the tray while offset is active. | Calibrated floor returns before the process exits. | |
| V10 | Quit SteamVR while TrueHeight is active. | Floor is restored, TrueHeight returns to waiting, and reconnects after SteamVR is restarted. | |
| V11 | Start OVR Advanced Settings but leave all Space Offset/Motion features off. | TrueHeight operates normally. | |
| G1 | Launch a representative seated SteamVR title. | Target height remains stable and game input continues normally. | |
| G2 | Crouch physically, then press the game's crouch input. | Physical height remains stabilized; the player crouches only when the game input is pressed. | |
| S1 | Run Microsoft Defender custom scan on the exact content directory. | No threat is reported. Record any detection name verbatim. | |
| S2 | Inspect file properties for TrueHeight.exe and openvr_api.dll. | Product version is 1.0.0; Valve's DLL signature is valid; app signature matches release decision. | |
| P1 | Install the depot through a protected Steam branch. | Exactly six expected content files install. | |
| P2 | Launch and quit through Steam. | Steam status changes Running -> stopped correctly. | |
| P3 | Validate installed files in Steam. | Validation succeeds and personal AppData settings are preserved. | |

## Failure handling

- If the floor moves away from the target, press F9 immediately. Record the
  headset/runtime, set invert=true, repeat V2, and do not ship until the default
  direction decision is documented.
- If F9 does not restore, exit TrueHeight and restart SteamVR. Do not continue
  the test session until the normal calibrated floor is confirmed.
- If Defender reports a threat, preserve the detection name and SHA-256, do not
  distribute that build, and submit the exact file through Microsoft's official
  false-positive process.
- Never test TrueHeight simultaneously with an enabled Space Offset, Motion,
  Height Toggle, or similar play-space mover.

Release QA result: PASS / FAIL
Owner approval:
