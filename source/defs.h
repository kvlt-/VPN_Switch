
#pragma once

#define DEF_APP_NAME            _T("VPN_Switch")
#define DEF_COMPANY_NAME        _T("kvlt")

#define USE_CONFIG_DIR          TRUE            // use DEF_CONFIG_DIR for config, instead of exe dir
#define DEF_CONFIG_DIR          _T("%APPDATA%\\") DEF_COMPANY_NAME

#define DEF_MAX_PROFILES        64              // max loaded OpenVPN profiles
#define DEF_OPENVPN_TIMEOUT     10              // max timeout of OpenVPN operations [s]
#define DEF_SHUTDOWN_TIMEOUT    4               // max timeout on app shutdown [s]


// custom window events

#define WM_TRAY_EVENT           (WM_USER+1)     // tray icon mouse clicked/hovered
#define WM_STATUS_EVENT         (WM_USER+2)     // connection status has been changed
#define WM_BYTECOUNT_EVENT      (WM_USER+3)     // connection traffic data have been updated
#define WM_SETTINGS_EVENT       (WM_USER+4)     // settings have been locked/unlocked

// main connection status
typedef enum
{
    VPN_ST_DISCONNECTED = 0,
    VPN_ST_CONNECTING,
    VPN_ST_CONNECTED,
    VPN_ST_DISCONNECTING,
    VPN_ST_EXITED,
    VPN_ST_ERROR
} VPN_STATUS;

// possible commands to controller/session
typedef enum
{
    VPN_CMD_NONE,
    VPN_CMD_CONNECT,
    VPN_CMD_DISCONNECT
} VPN_COMMAND;

// VPN profile definiton
typedef struct _VPN_PROFILE_T
{
    CString csName;
    CString csConfPath;

    BOOL Validate() { return !csName.IsEmpty() && !csConfPath.IsEmpty(); };
    void Reset() { csName.Empty(); csConfPath.Empty(); };
} VPN_PROFILE_T;


// helpful delete macros

#define CLOSEHANDLE(h)          if (h) { CloseHandle(h); h = NULL; }
#define DESTRUCT(x)             if (x) { delete x; x = NULL; }
#define DESTROYMENU(m)          if (m) { DestroyMenu(m); m = NULL; }
#define CLOSEFILE(f)            if (f != INVALID_HANDLE_VALUE) { CloseHandle(f); f = INVALID_HANDLE_VALUE; }

