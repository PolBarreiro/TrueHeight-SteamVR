# TrueHeight: Steam release checklist

Target price: **US$0.99** with Steam's recommended regional conversions.

## 1. Engineering readiness

- [x] Establish Git history and work on the steam-release branch.
- [x] Build the native Windows tray/settings application with MSVC x64.
- [x] Remove permanent chaperone commits; use only SteamVR's temporary
      working-set preview.
- [x] Make restore pause the loop so the restored floor stays restored.
- [x] Restore on clean exit and rebase after Room Setup/origin changes.
- [x] Remove unsupported SteamVR auto-start behavior.
- [x] Replace data-racy shared state with atomics.
- [x] Add deterministic control-loop tests and a Windows CI build.
- [x] Store personal configuration under %APPDATA%\TrueHeight.
- [x] Add a --no-vr UI smoke-test mode.
- [x] Replace the unverified stock icon with original project-owned artwork.
- [x] Create a reproducible Steam content build and checksum step.
- [ ] Test with the owner's real headset/runtime and confirm the default
      correction direction.
- [ ] Complete the clean-exit, F9, SteamVR-restart, headset-off, and Room Setup
      test matrix in QA-TEST-MATRIX.md.
- [ ] Scan the exact final content directory with Microsoft Defender.
- [ ] Submit a false-positive sample to Microsoft if the final MSVC binary is
      detected.
- [ ] Optional but recommended: sign TrueHeight.exe with an Authenticode code
      signing certificate, then rebuild checksums.

Do not submit the build to Valve before the hardware test passes. This utility
moves the user's perceived floor, so a desktop-only test is insufficient.

## 2. Steamworks onboarding: owner actions

- [ ] Sign in at https://partner.steamgames.com with Steam Guard enabled.
- [ ] Complete identity, tax, and bank information. For a Spanish individual,
      Steamworks will normally route the owner through its non-US tax interview.
- [ ] Pay the Steam Direct fee for this product: US$100 plus any applicable tax.
- [ ] Record the assigned AppID and Windows DepotID.

Timing to plan around:

- A new app cannot release until at least 30 days after the Steam Direct fee.
- The public Coming Soon page must be visible for at least two weeks.
- Valve says review usually takes 3-5 business days; submit at least 7 business
  days before the intended release.
- The Direct fee is recoupable after the product reaches US$1,000 adjusted gross
  revenue.

## 3. Steam application setup

- [ ] Product name: TrueHeight.
- [ ] Product/category: software utility (not a VR game).
- [ ] Supported OS: Windows 10/11, 64-bit.
- [ ] VR dependency: SteamVR-compatible headset and active tracking.
- [ ] Launch option:
  - Executable: TrueHeight.exe
  - OS: Windows
  - Architecture: 64-bit
  - Arguments: none
- [ ] Do not ship or register trueheight.vrmanifest; launching through Steam
      already associates the process with its AppID, and TrueHeight is not a
      dashboard overlay.
- [ ] Add the owner-approved EULA.txt and privacy notice.
- [ ] Add a public support email in Steamworks (not stored in this repository).

## 4. Store page

Use STORE-COPY.md as the editable English source. Keep these claims precise:

- Designed to work independently of the VR title; do not guarantee every game,
  headset, or runtime.
- Temporary SteamVR preview offset; no permanent Room Setup commit.
- No game injection and no game files modified.
- Physical crouching is intentionally cancelled while the lock is enabled.
- Do not run another play-space offset/motion tool simultaneously.

Recommended tags: Utilities, VR, Software, Accessibility.

Current Steam image sizes:

| Asset | Required canvas |
|---|---:|
| Header capsule | 920 x 430 |
| Small capsule | 462 x 174 |
| Main capsule | 1232 x 706 |
| Vertical capsule | 748 x 896 |
| Library capsule | 600 x 900 |
| Library hero | 3840 x 1240 |
| Library logo | 1280 px wide and/or 720 px high, transparent |
| Library header | 920 x 430 |
| Screenshots | At least 1920 x 1080 |

- [x] Create all required capsules from the new TrueHeight mark.
- [ ] Capture at least five truthful 1920x1080 screenshots from the release
      build, including settings, tray behavior, connected state, and the
      before/after VR result.
- [ ] Create a short demonstration trailer if practical.
- [ ] Keep capsule art free of review scores, discount copy, unrelated
      trademarks, and unsupported feature claims.
- [ ] Submit the store page for Valve review.
- [ ] Publish the approved Coming Soon page.

## 5. Price

- [ ] In Steamworks pricing, select the US$0.99 USD tier.
- [ ] Apply Steam's current recommended regional prices, then review them.
- [ ] Verify the store preview shows US$0.99 in the United States.

US$0.99 is Steam's supported minimum base-price tier. Very low prices may limit
discount options under Steam's minimum-transaction rules.

## 6. Build and upload

1. Run scripts/prepare-steam-build.ps1.
2. Confirm packaging/steam/content contains only:
   - TrueHeight.exe
   - openvr_api.dll
   - config.ini
   - README.txt
   - EULA.txt
   - LICENSE-OpenVR.txt
3. Review packaging/steam/checksums.sha256.
4. Replace APPID and DEPOTID placeholders in both VDF templates.
5. Keep Steamworks credentials out of the VDF files and repository.
6. From SteamCMD, run the app-build VDF with an authenticated builder account.
7. Assign the successful build to a password-protected test branch first.

- [ ] Install through Steam into a clean library folder.
- [ ] Verify launch and quit through the Steam client.
- [ ] Verify no VC++ redistributable is required.
- [ ] Verify Steam counts the app as stopped after choosing Quit from the tray.
- [ ] Verify update/reinstall preserves %APPDATA%\TrueHeight\config.ini.
- [ ] Promote the tested build to the default branch.
- [ ] Submit the build for Valve review.

## 7. Release gate

Release only when all are true:

- [ ] Store page and build reviews are approved.
- [ ] Coming Soon has been public for at least two weeks.
- [ ] Steam Direct 30-day wait has elapsed.
- [ ] Hardware QA matrix passes on the owner's headset.
- [ ] Final depot hash matches the tested build.
- [ ] Price preview is US$0.99.
- [ ] Support and privacy links work publicly.
- [ ] The owner has approved the release-date button in Steamworks.

## Official references

- Steam Direct fee: https://partner.steamgames.com/doc/gettingstarted/appfee
- Onboarding: https://partner.steamgames.com/doc/gettingstarted/onboarding
- Pricing: https://partner.steamgames.com/doc/store/pricing
- Store assets: https://partner.steamgames.com/doc/store/assets
- Store asset rules: https://partner.steamgames.com/doc/store/assets/rules
- Review and release: https://partner.steamgames.com/doc/store/releasing
- SteamPipe upload: https://partner.steamgames.com/doc/sdk/uploading
- SteamVR launch settings: https://partner.steamgames.com/doc/features/steamvr/settings
