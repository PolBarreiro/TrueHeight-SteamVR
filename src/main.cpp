// ============================================================================
//  TrueHeight v1.0.0  -  Automatic SteamVR floor-height lock
//
//  Steam-ready build: system-tray app with a settings window.
//  Control loop: continuously offsets the vertical origin of the SteamVR
//  standing tracking universe (chaperone) so the HMD always sits at a fixed
//  height above the play-area floor.
//
//  CLI:  TrueHeight.exe               normal start (tray)
//        TrueHeight.exe --no-vr       UI smoke test without connecting to VR
//        TrueHeight.exe --unregister  remove legacy auto-start registration
//
//  Global hotkeys: F7 reload config | F8 pause/resume | F9 restore real floor
//
//  Copyright (c) 2026 Pol Barreiro Font. All rights reserved.
//  Ships with openvr_api.dll (BSD-3-Clause, Valve Corporation).
// ============================================================================

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <commctrl.h>
#include <shlobj.h>
#include <mmsystem.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <cctype>
#include <cmath>
#include <atomic>
#include <string>
#include <fstream>
#include <algorithm>
#include <chrono>

#include "openvr.h"
#include "control.h"
#include "../res/resource.h"

// ---------------------------------------------------------------------------
// Constants / IDs
// ---------------------------------------------------------------------------
static const char* kAppName    = "TrueHeight";
static const char* kAppVersion = "1.0.0";
static const char* kLegacyAppKey = "pol.trueheight";
static const char* kWndClass   = "TrueHeightWnd";

#define WMAPP_TRAY   (WM_APP + 1)
#define WMAPP_SHOW   (WM_APP + 2)

enum {
    IDC_STATUS1 = 1001, IDC_STATUS2,
    IDC_BTN_TOGGLE = 1010, IDC_LBL_HEIGHT, IDC_EDIT_HEIGHT, IDC_TRACK,
    IDC_LBL_SMOOTH, IDC_EDIT_SMOOTH,
    IDC_LBL_DEAD,   IDC_EDIT_DEAD,
    IDC_LBL_SPEED,  IDC_EDIT_SPEED,
    IDC_CHK_WORN, IDC_CHK_INVERT, IDC_CHK_RESTORE, IDC_CHK_AUTOSTART, IDC_CHK_NECK,
    IDC_BTN_APPLY = 1030, IDC_BTN_RESTORE, IDC_HINT,
    IDM_TRAY_SHOW = 2001, IDM_TRAY_TOGGLE, IDM_TRAY_RESTORE, IDM_TRAY_QUIT
};

// ---------------------------------------------------------------------------
// Config
// ---------------------------------------------------------------------------
struct Config
{
    float target_height_cm  = 190.0f;
    float smoothing_time_ms = 800.0f;
    float deadzone_cm       = 8.0f;
    float max_speed_m_s     = 2.0f;
    float update_hz         = 60.0f;
    float max_offset_m      = 3.0f;
    bool  only_when_worn    = true;
    bool  start_enabled     = true;
    bool  restore_on_exit   = true;
    bool  invert            = false;
};

// ---------------------------------------------------------------------------
// Shared state (GUI thread <-> VR worker thread)
// ---------------------------------------------------------------------------
static CRITICAL_SECTION g_cfgLock;
static SRWLOCK          g_logLock = SRWLOCK_INIT;
static Config           g_cfg;                    // guarded by g_cfgLock

static std::atomic_bool  g_stop       { false };
static std::atomic_bool  g_enabled    { true };
static std::atomic_bool  g_connected  { false };
static std::atomic_bool  g_reqRestore { false };  // worker: restore real floor
static std::atomic_bool  g_reqReload  { false };  // worker: reload config file
static std::atomic<LONG> g_stateCode  { 0 };      // index into kStates
static std::atomic<float> g_heightCm  { 0.0f };
static std::atomic<float> g_offsetCm  { 0.0f };
static bool               g_noVr      = false;

static const char* kStates[] = {
    "WAITING FOR STEAMVR", "PAUSED", "LOCKED", "HEADSET OFF", "NO TRACKING",
    "FLOOR ACCESS ERROR"
};
enum { ST_WAITING = 0, ST_PAUSED, ST_LOCKED, ST_HMDOFF, ST_NOTRACK, ST_FLOORERROR };

static HWND g_hwnd = nullptr;

// ---------------------------------------------------------------------------
// Small helpers
// ---------------------------------------------------------------------------
static std::string ExeDir()
{
    char buf[MAX_PATH];
    GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string s(buf);
    size_t p = s.find_last_of("\\/");
    return (p == std::string::npos) ? std::string(".") : s.substr(0, p);
}

static std::string UserDataDir()
{
    char overrideDir[MAX_PATH];
    const DWORD overrideLength = GetEnvironmentVariableA(
        "TRUEHEIGHT_CONFIG_DIR", overrideDir, MAX_PATH);
    if (overrideLength > 0 && overrideLength < MAX_PATH)
    {
        CreateDirectoryA(overrideDir, nullptr);
        return overrideDir;
    }

    char appdata[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, appdata)))
    {
        std::string dir = std::string(appdata) + "\\TrueHeight";
        CreateDirectoryA(dir.c_str(), nullptr);
        return dir;
    }
    return ExeDir();  // fallback
}

static std::string ConfigPath()
{
    return UserDataDir() + "\\config.ini";
}

static std::string LogPath()
{
    return UserDataDir() + "\\TrueHeight.log";
}

static void LogMessage(const char* format, ...)
{
    char message[768] = {};
    va_list args;
    va_start(args, format);
    std::vsnprintf(message, sizeof(message), format, args);
    va_end(args);

    SYSTEMTIME now;
    GetLocalTime(&now);
    char timestamp[40] = {};
    std::snprintf(timestamp, sizeof(timestamp),
                  "%04u-%02u-%02u %02u:%02u:%02u.%03u",
                  now.wYear, now.wMonth, now.wDay,
                  now.wHour, now.wMinute, now.wSecond, now.wMilliseconds);

    AcquireSRWLockExclusive(&g_logLock);
    std::ofstream file(LogPath(), std::ios::app);
    if (file.good()) file << timestamp << "  " << message << '\n';
    ReleaseSRWLockExclusive(&g_logLock);
}

static std::string Trim(const std::string& in)
{
    size_t a = in.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = in.find_last_not_of(" \t\r\n");
    return in.substr(a, b - a + 1);
}

static float ParseFloat(std::string v, float fallback)
{
    std::replace(v.begin(), v.end(), ',', '.');   // ES locale friendliness
    char* end = nullptr;
    float f = strtof(v.c_str(), &end);
    return (end && end != v.c_str()) ? f : fallback;
}

static bool ParseBool(const std::string& v, bool fallback)
{
    std::string s = v;
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    if (s == "true"  || s == "1" || s == "yes" || s == "on")  return true;
    if (s == "false" || s == "0" || s == "no"  || s == "off") return false;
    return fallback;
}

static float Clampf(float v, float lo, float hi)
{
    return trueheight::Clamp(v, lo, hi);
}

static void SanitizeConfig(Config& c)
{
    c.target_height_cm  = Clampf(c.target_height_cm, 100.0f, 230.0f);
    c.smoothing_time_ms = Clampf(c.smoothing_time_ms, 50.0f, 5000.0f);
    c.deadzone_cm       = Clampf(c.deadzone_cm, 0.0f, 50.0f);
    c.max_speed_m_s     = Clampf(c.max_speed_m_s, 0.1f, 5.0f);
    c.update_hz         = Clampf(c.update_hz, 10.0f, 144.0f);
    c.max_offset_m      = Clampf(c.max_offset_m, 0.5f, 5.0f);
}

static bool SaveConfig(const Config& c)
{
    std::ofstream f(ConfigPath());
    if (!f.good()) return false;
    f << "; TrueHeight " << kAppVersion << " config (auto-generated)\n"
      << "target_height_cm="  << c.target_height_cm  << "\n"
      << "smoothing_time_ms=" << c.smoothing_time_ms << "\n"
      << "deadzone_cm="       << c.deadzone_cm       << "\n"
      << "max_speed_m_s="     << c.max_speed_m_s     << "\n"
      << "update_hz="         << c.update_hz         << "\n"
      << "max_offset_m="      << c.max_offset_m      << "\n"
      << "only_when_worn="    << (c.only_when_worn  ? "true" : "false") << "\n"
      << "start_enabled="     << (c.start_enabled   ? "true" : "false") << "\n"
      << "restore_on_exit="   << (c.restore_on_exit ? "true" : "false") << "\n"
      << "invert="            << (c.invert          ? "true" : "false") << "\n";
    return f.good();
}

static bool LoadConfigFile(const std::string& path, Config& cfg)
{
    std::ifstream f(path);
    if (!f.good()) return false;
    std::string line;
    while (std::getline(f, line))
    {
        size_t c = line.find_first_of(";#");
        if (c != std::string::npos) line = line.substr(0, c);
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = Trim(line.substr(0, eq));
        std::string val = Trim(line.substr(eq + 1));
        if (key.empty() || val.empty()) continue;

        if      (key == "target_height_cm")  cfg.target_height_cm  = ParseFloat(val, cfg.target_height_cm);
        else if (key == "smoothing_time_ms") cfg.smoothing_time_ms = ParseFloat(val, cfg.smoothing_time_ms);
        else if (key == "deadzone_cm")       cfg.deadzone_cm       = ParseFloat(val, cfg.deadzone_cm);
        else if (key == "max_speed_m_s")     cfg.max_speed_m_s     = ParseFloat(val, cfg.max_speed_m_s);
        else if (key == "update_hz")         cfg.update_hz         = ParseFloat(val, cfg.update_hz);
        else if (key == "max_offset_m")      cfg.max_offset_m      = ParseFloat(val, cfg.max_offset_m);
        else if (key == "only_when_worn")    cfg.only_when_worn    = ParseBool(val, cfg.only_when_worn);
        else if (key == "start_enabled")     cfg.start_enabled     = ParseBool(val, cfg.start_enabled);
        else if (key == "restore_on_exit")   cfg.restore_on_exit   = ParseBool(val, cfg.restore_on_exit);
        else if (key == "invert")            cfg.invert            = ParseBool(val, cfg.invert);
    }
    SanitizeConfig(cfg);
    return true;
}

static void LoadConfig(Config& cfg)
{
    const std::string userPath = ConfigPath();
    if (LoadConfigFile(userPath, cfg)) return;

    // Steam installs are commonly read-only for standard users. Seed the
    // writable per-user config from the file shipped beside the executable,
    // then use the per-user copy for all later reads and writes.
    LoadConfigFile(ExeDir() + "\\config.ini", cfg);
    if (!SaveConfig(cfg))
        LogMessage("Unable to create configuration: %s", userPath.c_str());
}

// ---------------------------------------------------------------------------
// Chaperone manipulation (worker thread only)
// ---------------------------------------------------------------------------
static bool CaptureFloorPose(vr::HmdMatrix34_t& pose)
{
    vr::IVRChaperoneSetup* cs = vr::VRChaperoneSetup();
    if (!cs) return false;
    cs->RevertWorkingCopy();
    return cs->GetWorkingStandingZeroPoseToRawTrackingPose(&pose);
}

static bool ApplyFloorOffset(const vr::HmdMatrix34_t& baseline,
                             float perceivedHeightOffset,
                             bool invert)
{
    vr::IVRChaperoneSetup* cs = vr::VRChaperoneSetup();
    if (!cs) return false;

    // Never commit the user's live room calibration. Apply an absolute offset
    // from the captured baseline through SteamVR's temporary working preview.
    // This is the same mechanism used for normal playspace motion by OVRAS.
    vr::HmdMatrix34_t adjusted = baseline;
    const float matrixOffset = invert ? perceivedHeightOffset : -perceivedHeightOffset;
    for (int row = 0; row < 3; ++row)
        adjusted.m[row][3] += baseline.m[row][1] * matrixOffset;

    cs->SetWorkingStandingZeroPoseToRawTrackingPose(&adjusted);
    cs->ShowWorkingSetPreview();
    return true;
}

static void RestoreFloor()
{
    vr::IVRChaperoneSetup* cs = vr::VRChaperoneSetup();
    if (!cs) return;
    cs->RevertWorkingCopy();
    cs->HideWorkingSetPreview();
}

// ---------------------------------------------------------------------------
// VR worker thread: connect / control loop / reconnect
// ---------------------------------------------------------------------------
static DWORD WINAPI VrWorker(LPVOID)
{
    using SClock = std::chrono::steady_clock;

    float appliedOffset = 0.0f;
    vr::HmdMatrix34_t floorBaseline = {};
    bool floorBaselineValid = false;
    bool hmdHasProximitySensor = false;
    vr::EVRInitError lastInitError = vr::VRInitError_None;
    bool  pF7 = false, pF8 = false, pF9 = false;
    auto  tPrev = SClock::now();

    while (!g_stop.load())
    {
        // ---- (Re)connect to SteamVR without force-launching it -------------
        if (!g_connected.load())
        {
            g_stateCode.store(ST_WAITING);
            vr::EVRInitError ierr = vr::VRInitError_None;
            vr::VR_Init(&ierr, vr::VRApplication_Background);
            if (ierr != vr::VRInitError_None)
            {
                if (ierr != lastInitError)
                    LogMessage("SteamVR connection failed: %s",
                               vr::VR_GetVRInitErrorAsEnglishDescription(ierr));
                lastInitError = ierr;
                for (int i = 0; i < 20 && !g_stop.load(); ++i) Sleep(100);
                continue;
            }
            // Clear any working-set preview left by a previous unclean exit,
            // then capture the real live floor as this session's baseline.
            RestoreFloor();
            floorBaselineValid = CaptureFloorPose(floorBaseline);
            lastInitError = vr::VRInitError_None;
            g_connected.store(true);
            appliedOffset = 0.0f;
            vr::ETrackedPropertyError propError = vr::TrackedProp_Success;
            hmdHasProximitySensor = vr::VRSystem()->GetBoolTrackedDeviceProperty(
                vr::k_unTrackedDeviceIndex_Hmd,
                vr::Prop_ContainsProximitySensor_Bool,
                &propError) && propError == vr::TrackedProp_Success;
            LogMessage("Connected to SteamVR; floor access=%s; proximity sensor=%s",
                       floorBaselineValid ? "ok" : "failed",
                       hmdHasProximitySensor ? "yes" : "no");
            tPrev = SClock::now();
        }

        // ---- SteamVR events -------------------------------------------------
        bool steamvrQuit = false;
        bool floorChanged = false;
        vr::VREvent_t ev;
        while (vr::VRSystem() && vr::VRSystem()->PollNextEvent(&ev, sizeof(ev)))
        {
            if (ev.eventType == vr::VREvent_Quit)
            {
                vr::VRSystem()->AcknowledgeQuit_Exiting();
                steamvrQuit = true;
            }
            else if (ev.eventType == vr::VREvent_StandingZeroPoseReset ||
                     ev.eventType == vr::VREvent_ChaperoneUniverseHasChanged ||
                     ev.eventType == vr::VREvent_ChaperoneRoomSetupCommitted)
            {
                floorChanged = true;
            }
        }
        if (steamvrQuit)
        {
            LogMessage("SteamVR requested shutdown; restoring floor");
            RestoreFloor();
            vr::VR_Shutdown();
            g_connected.store(false);
            g_offsetCm.store(0.0f);
            floorBaselineValid = false;
            hmdHasProximitySensor = false;
            Sleep(2000);
            continue;
        }
        if (floorChanged)
        {
            RestoreFloor();
            floorBaselineValid = CaptureFloorPose(floorBaseline);
            appliedOffset = 0.0f;
            LogMessage("SteamVR standing origin changed; floor baseline rebased (%s)",
                       floorBaselineValid ? "ok" : "failed");
        }

        // ---- Copy config ----------------------------------------------------
        Config cfg;
        EnterCriticalSection(&g_cfgLock);
        cfg = g_cfg;
        LeaveCriticalSection(&g_cfgLock);
        // ---- Global hotkeys -------------------------------------------------
        bool f7 = (GetAsyncKeyState(VK_F7) & 0x8000) != 0;
        bool f8 = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
        bool f9 = (GetAsyncKeyState(VK_F9) & 0x8000) != 0;
        if (f7 && !pF7) g_reqReload.store(true);
        if (f8 && !pF8) g_enabled.store(!g_enabled.load());
        if (f9 && !pF9) g_reqRestore.store(true);
        pF7 = f7; pF8 = f8; pF9 = f9;

        // ---- Requested actions ----------------------------------------------
        if (g_reqReload.exchange(false))
        {
            Config fresh; LoadConfig(fresh);
            EnterCriticalSection(&g_cfgLock);
            g_cfg = fresh;
            LeaveCriticalSection(&g_cfgLock);
            cfg = fresh;
            LogMessage("Configuration reloaded");
        }
        if (g_reqRestore.exchange(false))
        {
            // "Restore" must be durable. Pausing prevents the control loop
            // from re-applying the offset on the very next frame.
            g_enabled.store(false);
            RestoreFloor();
            floorBaselineValid = CaptureFloorPose(floorBaseline);
            appliedOffset = 0.0f;
            LogMessage("Floor restored and height lock paused (%s)",
                       floorBaselineValid ? "ok" : "floor access failed");
        }

        // ---- Timing ---------------------------------------------------------
        auto  tNow = SClock::now();
        float dt   = std::chrono::duration<float>(tNow - tPrev).count();
        tPrev = tNow;
        dt = std::min(dt, 0.1f);

        // ---- Control loop ---------------------------------------------------
        LONG state = g_enabled.load() ? ST_LOCKED : ST_PAUSED;

        if (g_enabled.load() && !floorBaselineValid)
        {
            state = ST_FLOORERROR;
        }
        else if (g_enabled.load() && vr::VRSystem())
        {
            vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
            vr::VRSystem()->GetDeviceToAbsoluteTrackingPose(
                vr::TrackingUniverseStanding, 0.0f, poses, vr::k_unMaxTrackedDeviceCount);
            const vr::TrackedDevicePose_t& hmd = poses[vr::k_unTrackedDeviceIndex_Hmd];

            bool worn = true;
            if (cfg.only_when_worn && hmdHasProximitySensor)
            {
                const vr::EDeviceActivityLevel activity =
                    vr::VRSystem()->GetTrackedDeviceActivityLevel(vr::k_unTrackedDeviceIndex_Hmd);
                worn = activity == vr::k_EDeviceActivityLevel_UserInteraction ||
                       activity == vr::k_EDeviceActivityLevel_UserInteraction_Timeout;
            }

            if (!worn)
            {
                state = ST_HMDOFF;
            }
            else if (!hmd.bPoseIsValid || hmd.eTrackingResult != vr::TrackingResult_Running_OK)
            {
                state = ST_NOTRACK;
            }
            else
            {
                float heightM = hmd.mDeviceToAbsoluteTracking.m[1][3];
                g_heightCm.store(heightM * 100.0f);

                const trueheight::ControlSettings control {
                    cfg.target_height_cm * 0.01f,
                    cfg.smoothing_time_ms * 0.001f,
                    cfg.deadzone_cm * 0.01f,
                    cfg.max_speed_m_s,
                    cfg.max_offset_m
                };
                const float nextOffset = trueheight::ComputeNextOffset(
                    heightM, appliedOffset, dt, control);
                if (std::fabs(nextOffset - appliedOffset) > 0.0002f && floorBaselineValid)
                {
                    if (ApplyFloorOffset(floorBaseline, nextOffset, cfg.invert))
                        appliedOffset = nextOffset;
                }
            }
        }
        g_stateCode.store(state);
        g_offsetCm.store(appliedOffset * 100.0f);

        // ---- Sleep to target rate -------------------------------------------
        float period = 1.0f / std::max(10.0f, cfg.update_hz);
        float spent  = std::chrono::duration<float>(SClock::now() - tNow).count();
        int   ms     = (int)((period - spent) * 1000.0f);
        if (ms > 0) Sleep((DWORD)ms);
    }

    // ---- Clean exit ----------------------------------------------------------
    if (g_connected.load())
    {
        // Preview offsets must never outlive this process, regardless of the
        // legacy restore_on_exit setting.
        RestoreFloor();
        vr::VR_Shutdown();
        g_connected.store(false);
        LogMessage("SteamVR disconnected after clean floor restore");
    }
    return 0;
}

// ---------------------------------------------------------------------------
// GUI
// ---------------------------------------------------------------------------
static NOTIFYICONDATAA g_nid = {};
static bool  g_balloonShown = false;
static HWND MakeCtl(HWND parent, const char* cls, const char* text, DWORD style,
                    int x, int y, int w, int h, int id)
{
    return CreateWindowExA(0, cls, text, WS_CHILD | WS_VISIBLE | style,
                           x, y, w, h, parent, (HMENU)(INT_PTR)id,
                           GetModuleHandleA(nullptr), nullptr);
}

static void UpdateToggleButton(HWND h)
{
    SetWindowTextA(GetDlgItem(h, IDC_BTN_TOGGLE),
                   g_enabled.load() ? "STOP height lock  (F8)" : "START height lock  (F8)");
}

static BOOL CALLBACK SetFontProc(HWND hwnd, LPARAM lp)
{
    SendMessageA(hwnd, WM_SETFONT, (WPARAM)lp, TRUE);
    return TRUE;
}

static void CreateControls(HWND h)
{
    MakeCtl(h, "STATIC", "SteamVR: waiting...", 0, 12, 12, 356, 18, IDC_STATUS1);
    MakeCtl(h, "STATIC", "", 0, 12, 32, 356, 18, IDC_STATUS2);

    MakeCtl(h, "STATIC", "Target height (cm):", 0, 12, 92, 200, 18, IDC_LBL_HEIGHT);
    MakeCtl(h, "EDIT", "190", WS_BORDER | ES_AUTOHSCROLL, 300, 88, 64, 22, IDC_EDIT_HEIGHT);

    HWND track = CreateWindowExA(0, TRACKBAR_CLASSA, "", WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_AUTOTICKS,
                                 12, 114, 352, 30, h, (HMENU)(INT_PTR)IDC_TRACK,
                                 GetModuleHandleA(nullptr), nullptr);
    SendMessageA(track, TBM_SETRANGE, TRUE, MAKELPARAM(120, 220));
    SendMessageA(track, TBM_SETTICFREQ, 10, 0);
    SendMessageA(track, TBM_SETPOS, TRUE, 190);

    MakeCtl(h, "STATIC", "Smoothing (ms)  -  low = tight lock:", 0, 12, 156, 270, 18, IDC_LBL_SMOOTH);
    MakeCtl(h, "EDIT", "300", WS_BORDER | ES_AUTOHSCROLL, 300, 152, 64, 22, IDC_EDIT_SMOOTH);
    MakeCtl(h, "STATIC", "Deadzone (cm):", 0, 12, 184, 270, 18, IDC_LBL_DEAD);
    MakeCtl(h, "EDIT", "1.5", WS_BORDER | ES_AUTOHSCROLL, 300, 180, 64, 22, IDC_EDIT_DEAD);
    MakeCtl(h, "STATIC", "Max correction speed (m/s):", 0, 12, 212, 270, 18, IDC_LBL_SPEED);
    MakeCtl(h, "EDIT", "1.0", WS_BORDER | ES_AUTOHSCROLL, 300, 208, 64, 22, IDC_EDIT_SPEED);

    MakeCtl(h, "BUTTON", "Only adjust while the headset is worn", BS_AUTOCHECKBOX, 12, 244, 352, 20, IDC_CHK_WORN);
    MakeCtl(h, "BUTTON", "Invert correction direction (only if floor runs away)", BS_AUTOCHECKBOX, 12, 268, 352, 20, IDC_CHK_INVERT);
    MakeCtl(h, "STATIC", "Safety: the real floor is always restored on exit.", 0, 12, 292, 352, 20, IDC_CHK_RESTORE);
    MakeCtl(h, "STATIC", "Launch TrueHeight from Steam before starting your VR game.", 0, 12, 316, 352, 20, IDC_CHK_AUTOSTART);

    MakeCtl(h, "BUTTON", "Apply && Save", BS_PUSHBUTTON, 12, 348, 150, 28, IDC_BTN_APPLY);
    MakeCtl(h, "BUTTON", "Restore real floor now  (F9)", BS_PUSHBUTTON, 174, 348, 190, 28, IDC_BTN_RESTORE);

    MakeCtl(h, "STATIC",
            "Sit, stand or lie back freely - your in-game height never\n"
            "changes. Stand up when combat starts; nothing to adjust.\n"
            "\n"
            "Trade-off: because the floor follows you, crouching\n"
            "physically now reads as sitting down. Use your game's\n"
            "crouch button to crouch.\n"
            "\n"
            "In-game hotkeys: F7 reload | F8 pause/resume | F9 restore.\n"
            "Closing this window keeps TrueHeight running in the tray.",
            0, 12, 386, 356, 122, IDC_HINT);

    MakeCtl(h, "BUTTON", "STOP height lock  (F8)", BS_PUSHBUTTON | WS_TABSTOP,
            12, 520, 352, 48, IDC_BTN_TOGGLE);

    EnumChildWindows(h, SetFontProc, (LPARAM)GetStockObject(DEFAULT_GUI_FONT));
}

static void GuiFromConfig(HWND h)
{
    Config c;
    EnterCriticalSection(&g_cfgLock);
    c = g_cfg;
    LeaveCriticalSection(&g_cfgLock);

    char b[64];
    std::snprintf(b, sizeof(b), "%.0f", c.target_height_cm);  SetWindowTextA(GetDlgItem(h, IDC_EDIT_HEIGHT), b);
    std::snprintf(b, sizeof(b), "%.0f", c.smoothing_time_ms); SetWindowTextA(GetDlgItem(h, IDC_EDIT_SMOOTH), b);
    std::snprintf(b, sizeof(b), "%.1f", c.deadzone_cm);       SetWindowTextA(GetDlgItem(h, IDC_EDIT_DEAD), b);
    std::snprintf(b, sizeof(b), "%.1f", c.max_speed_m_s);     SetWindowTextA(GetDlgItem(h, IDC_EDIT_SPEED), b);
    SendMessageA(GetDlgItem(h, IDC_TRACK), TBM_SETPOS, TRUE, (LPARAM)(LONG)(c.target_height_cm + 0.5f));
    CheckDlgButton(h, IDC_CHK_WORN,    c.only_when_worn  ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(h, IDC_CHK_INVERT,  c.invert          ? BST_CHECKED : BST_UNCHECKED);
    UpdateToggleButton(h);
}

static float ReadEditFloat(HWND h, int id, float fallback)
{
    char b[64] = {};
    GetWindowTextA(GetDlgItem(h, id), b, sizeof(b) - 1);
    return ParseFloat(b, fallback);
}

static void ApplyFromGui(HWND h)
{
    Config c;
    EnterCriticalSection(&g_cfgLock);
    c = g_cfg;
    LeaveCriticalSection(&g_cfgLock);

    c.target_height_cm  = ReadEditFloat(h, IDC_EDIT_HEIGHT, c.target_height_cm);
    c.smoothing_time_ms = ReadEditFloat(h, IDC_EDIT_SMOOTH, c.smoothing_time_ms);
    c.deadzone_cm       = ReadEditFloat(h, IDC_EDIT_DEAD,   c.deadzone_cm);
    c.max_speed_m_s     = ReadEditFloat(h, IDC_EDIT_SPEED,  c.max_speed_m_s);
    c.only_when_worn    = IsDlgButtonChecked(h, IDC_CHK_WORN)    == BST_CHECKED;
    c.invert            = IsDlgButtonChecked(h, IDC_CHK_INVERT)  == BST_CHECKED;
    c.restore_on_exit   = true; // Kept in the config format for backward compatibility.
    SanitizeConfig(c);

    EnterCriticalSection(&g_cfgLock);
    g_cfg = c;
    LeaveCriticalSection(&g_cfgLock);
    if (!SaveConfig(c))
    {
        MessageBoxA(h, "Could not save settings to the TrueHeight AppData folder.",
                    kAppName, MB_OK | MB_ICONERROR);
        LogMessage("Unable to save configuration from the settings window");
    }
    else
    {
        LogMessage("Settings saved: target=%.1f cm, smoothing=%.0f ms, deadzone=%.1f cm",
                   c.target_height_cm, c.smoothing_time_ms, c.deadzone_cm);
    }
    GuiFromConfig(h);   // reflect clamped values
}

static void TrayAdd(HWND h)
{
    g_nid = {};
    g_nid.cbSize = sizeof(g_nid);
    g_nid.hWnd = h;
    g_nid.uID = 1;
    g_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_nid.uCallbackMessage = WMAPP_TRAY;
    g_nid.hIcon = (HICON)LoadImageA(GetModuleHandleA(nullptr), MAKEINTRESOURCEA(IDI_APPICON),
                                    IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    std::snprintf(g_nid.szTip, sizeof(g_nid.szTip),
                  "%s", "TrueHeight - double-click to open");
    Shell_NotifyIconA(NIM_ADD, &g_nid);
}

static void TrayBalloonOnce()
{
    if (g_balloonShown) return;
    g_balloonShown = true;
    g_nid.uFlags |= NIF_INFO;
    std::snprintf(g_nid.szInfoTitle, sizeof(g_nid.szInfoTitle),
                  "%s", "TrueHeight is still running");
    std::snprintf(g_nid.szInfo, sizeof(g_nid.szInfo),
                  "%s", "The height lock stays active in the system tray.");
    g_nid.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIconA(NIM_MODIFY, &g_nid);
    g_nid.uFlags &= ~NIF_INFO;
}

static void UpdateStatus(HWND h)
{
    char b[160];
    LONG st = g_stateCode.load();

    if (g_noVr)
        SetWindowTextA(GetDlgItem(h, IDC_STATUS1), "SteamVR: diagnostic mode (not connected)");
    else
        SetWindowTextA(GetDlgItem(h, IDC_STATUS1),
                       g_connected.load() ? "SteamVR: connected" : "SteamVR: not running - waiting...");

    if (st == ST_LOCKED)
        std::snprintf(b, sizeof(b), "Height: %.1f cm     Floor offset: %+.1f cm     [%s]",
                      g_heightCm.load(), g_offsetCm.load(), kStates[st]);
    else
        std::snprintf(b, sizeof(b), "Floor offset: %+.1f cm     [%s]",
                      g_offsetCm.load(), kStates[st]);
    SetWindowTextA(GetDlgItem(h, IDC_STATUS2), b);

    // Reflect F8 and tray toggles in the action button.
    UpdateToggleButton(h);

}

static void ShowTrayMenu(HWND h)
{
    HMENU m = CreatePopupMenu();
    AppendMenuA(m, MF_STRING, IDM_TRAY_SHOW, "Settings");
    AppendMenuA(m, MF_STRING | (g_enabled.load() ? MF_CHECKED : 0), IDM_TRAY_TOGGLE, "Height lock enabled");
    AppendMenuA(m, MF_STRING, IDM_TRAY_RESTORE, "Restore real floor");
    AppendMenuA(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(m, MF_STRING, IDM_TRAY_QUIT, "Quit");
    POINT p; GetCursorPos(&p);
    SetForegroundWindow(h);
    TrackPopupMenu(m, TPM_RIGHTBUTTON, p.x, p.y, 0, h, nullptr);
    PostMessageA(h, WM_NULL, 0, 0);
    DestroyMenu(m);
}

static LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_CREATE:
        CreateControls(h);
        GuiFromConfig(h);
        TrayAdd(h);
        SetTimer(h, 1, 250, nullptr);
        return 0;

    case WM_TIMER:
        UpdateStatus(h);
        return 0;

    case WM_HSCROLL:
        if ((HWND)lp == GetDlgItem(h, IDC_TRACK))
        {
            LONG pos = (LONG)SendMessageA((HWND)lp, TBM_GETPOS, 0, 0);
            char b[16]; std::snprintf(b, sizeof(b), "%ld", (long)pos);
            SetWindowTextA(GetDlgItem(h, IDC_EDIT_HEIGHT), b);
            EnterCriticalSection(&g_cfgLock);
            g_cfg.target_height_cm = (float)pos;   // live apply while sliding
            LeaveCriticalSection(&g_cfgLock);
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wp))
        {
        case IDC_BTN_TOGGLE:
            g_enabled.store(!g_enabled.load());
            UpdateToggleButton(h);
            return 0;
        case IDC_BTN_APPLY:
            ApplyFromGui(h);
            return 0;
        case IDC_BTN_RESTORE:
        case IDM_TRAY_RESTORE:
            g_reqRestore.store(true);
            return 0;
        case IDM_TRAY_SHOW:
            ShowWindow(h, SW_SHOW); ShowWindow(h, SW_RESTORE); SetForegroundWindow(h);
            return 0;
        case IDM_TRAY_TOGGLE:
            g_enabled.store(!g_enabled.load());
            return 0;
        case IDM_TRAY_QUIT:
            DestroyWindow(h);
            return 0;
        }
        break;

    case WMAPP_TRAY:
        if (lp == WM_LBUTTONDBLCLK)
        { ShowWindow(h, SW_SHOW); ShowWindow(h, SW_RESTORE); SetForegroundWindow(h); }
        else if (lp == WM_RBUTTONUP)
            ShowTrayMenu(h);
        return 0;

    case WMAPP_SHOW:
        ShowWindow(h, SW_SHOW); ShowWindow(h, SW_RESTORE); SetForegroundWindow(h);
        return 0;

    case WM_CLOSE:                 // close button -> hide to tray
        ShowWindow(h, SW_HIDE);
        TrayBalloonOnce();
        return 0;

    case WM_DESTROY:
        KillTimer(h, 1);
        g_stop.store(true);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(h, msg, wp, lp);
}

// ---------------------------------------------------------------------------
// CLI registration mode (no SteamVR-side GUI needed)
// ---------------------------------------------------------------------------
static int CliRegistration(bool install)
{
    if (install)
    {
        MessageBoxA(nullptr,
            "SteamVR auto-start registration is not supported by this version.\n\n"
            "Launch TrueHeight from Steam before starting your VR game.",
            kAppName, MB_OK | MB_ICONINFORMATION);
        return 2;
    }

    vr::EVRInitError err = vr::VRInitError_None;
    vr::VR_Init(&err, vr::VRApplication_Utility);
    if (err != vr::VRInitError_None)
    {
        MessageBoxA(nullptr, vr::VR_GetVRInitErrorAsEnglishDescription(err), kAppName, MB_ICONERROR);
        return 1;
    }
    std::string manifest = ExeDir() + "\\trueheight.vrmanifest";
    if (vr::VRApplications())
    {
        vr::VRApplications()->SetApplicationAutoLaunch(kLegacyAppKey, false);
        vr::VRApplications()->RemoveApplicationManifest(manifest.c_str());
    }
    MessageBoxA(nullptr, "Legacy SteamVR auto-start registration removed.", kAppName, MB_OK);
    vr::VR_Shutdown();
    return 0;
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int WINAPI WinMain(_In_ HINSTANCE hInst, _In_opt_ HINSTANCE hPrevInstance,
                   _In_ LPSTR lpCmdLine, _In_ int nShowCmd)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(nShowCmd);
    if (lpCmdLine && strstr(lpCmdLine, "--register"))   return CliRegistration(true);
    if (lpCmdLine && strstr(lpCmdLine, "--unregister")) return CliRegistration(false);
    g_noVr = lpCmdLine && strstr(lpCmdLine, "--no-vr");

    // single instance: focus the existing window instead
    HANDLE mutex = CreateMutexA(nullptr, TRUE, "Local\\TrueHeightSingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        HWND prev = FindWindowA(kWndClass, nullptr);
        if (prev) PostMessageA(prev, WMAPP_SHOW, 0, 0);
        return 0;
    }

    InitializeCriticalSection(&g_cfgLock);
    LoadConfig(g_cfg);
    g_enabled.store(g_cfg.start_enabled);
    LogMessage("TrueHeight %s started%s", kAppVersion,
               g_noVr ? " in no-VR diagnostic mode" : "");

    timeBeginPeriod(1);

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_BAR_CLASSES };
    InitCommonControlsEx(&icc);

    WNDCLASSA wc = {};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = kWndClass;
    wc.hIcon         = LoadIconA(hInst, MAKEINTRESOURCEA(IDI_APPICON));
    wc.hCursor       = LoadCursorA(nullptr, MAKEINTRESOURCEA(32512)); // IDC_ARROW
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    RegisterClassA(&wc);

    RECT r = { 0, 0, 376, 580 };
    AdjustWindowRect(&r, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
    char title[64];
    std::snprintf(title, sizeof(title), "TrueHeight %s", kAppVersion);
    g_hwnd = CreateWindowExA(0, kWndClass, title,
                             WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                             CW_USEDEFAULT, CW_USEDEFAULT,
                             r.right - r.left, r.bottom - r.top,
                             nullptr, nullptr, hInst, nullptr);
    ShowWindow(g_hwnd, SW_SHOW);

    HANDLE worker = g_noVr ? nullptr : CreateThread(nullptr, 0, VrWorker, nullptr, 0, nullptr);
    if (!g_noVr && !worker)
    {
        LogMessage("VR worker thread could not be created (Windows error %lu)", GetLastError());
        MessageBoxA(g_hwnd, "The SteamVR worker could not start. Please quit and try again.",
                    kAppName, MB_OK | MB_ICONERROR);
    }

    MSG msg;
    while (GetMessageA(&msg, nullptr, 0, 0) > 0)
    {
        if (!IsDialogMessageA(g_hwnd, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }

    // shutdown: worker restores the floor and disconnects
    g_stop.store(true);
    if (worker)
    {
        WaitForSingleObject(worker, 5000);
        CloseHandle(worker);
    }
    Shell_NotifyIconA(NIM_DELETE, &g_nid);
    timeEndPeriod(1);
    DeleteCriticalSection(&g_cfgLock);
    if (mutex) { ReleaseMutex(mutex); CloseHandle(mutex); }
    LogMessage("TrueHeight process exited");
    return 0;
}
