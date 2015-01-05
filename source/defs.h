
#pragma once

#define USE_CONFIG_DIR          // use %APPDATA%\DEF_COMPANY_NAME dir for config, instead of exe dir

#define DEF_APP_NAME            _T("VPN_Switch")
#define DEF_COMPANY_NAME        _T("kvlt")

#define DEF_MAX_PROFILES        128             // max loaded OpenVPN profiles
#define DEF_OPENVPN_TIMEOUT     10              // max timeout of OpenVPN operations [s]

#define DEF_PORT_MIN            1025            // min OpenVPN management port
#define DEF_PORT_MAX            65535           // max OpenVPN management port
#define DEF_PASSWORD_MIN        4               // min OpenVPN management password length
#define DEF_PASSWORD_MAX        12              // max OpenVPN management password length

// TODO: log window

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
typedef struct
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

// getter/setter macros

#define DECLARE_GETTER_REF(Type,Fnc,Member) \
    __inline const Type& Fnc() const \
{ \
    return Member; \
}
#define DECLARE_GETTER_VAL(Type,Fnc,Member) \
    __inline Type Fnc() const \
{ \
    return Member; \
}
#define DECLARE_GETTER_REF_LOCK(Type,Fnc,Member) \
    __inline void Fnc(Type &value) \
{ \
    EnterCriticalSection(&m_crsMain); \
    value = Member; \
    LeaveCriticalSection(&m_crsMain); \
}
#define DECLARE_GETTER_VAL_LOCK(Type,Fnc,Member) \
    __inline const Type Fnc() \
{ \
    Type value; \
    EnterCriticalSection(&m_crsMain); \
    value = Member; \
    LeaveCriticalSection(&m_crsMain); \
    return value; \
}
#define DECLARE_SETTER_REF_LOCK(Type,Fnc,Member) \
    __inline void Fnc(Type &value) \
{ \
    EnterCriticalSection(&m_crsMain); \
    Member = value; \
    LeaveCriticalSection(&m_crsMain); \
}
#define DECLARE_SETTER_VAL_LOCK(Type,Fnc,Member) \
    __inline void Fnc(Type value) \
{ \
    EnterCriticalSection(&m_crsMain); \
    Member = value; \
    LeaveCriticalSection(&m_crsMain); \
}