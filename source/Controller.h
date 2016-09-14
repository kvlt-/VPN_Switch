
#pragma once

#include "Configuration.h"
#include "ManagementSession.h"
#include "DNSLeaks.h"

class CController
{
public:
    typedef struct _TRAFFIC_DATA_T
    {
        ULONGLONG ullTotalInKb;
        ULONGLONG ullTotalOutKb;
        DWORD dwSpeedInKbs;
        DWORD dwSpeedOutKbs;
        ULONGLONG ullTotalTime;
    } TRAFFIC_DATA_T;

public:
    CController(CConfiguration *pConfig, HWND hMainWnd);
    ~CController();

    BOOL Start();
    void Stop(BOOL bExiting);
    void WaitForStop();

    VPN_STATUS GetStatus(PUINT puiErrorID = NULL);
    CString GetActiveProfileName();
    CString GetActiveProfileConfPath();
    CString GetExternalIP();
    CController::TRAFFIC_DATA_T GetTrafficData();

    void SetCommand(VPN_COMMAND command);
    void SetActiveProfile(UINT uiProfile);
    void SetAuthInfo(LPCTSTR szUser, LPCTSTR szPassword);
    void SetAuthCancel();

protected:
    typedef struct _STATE_T
    {
        CRITICAL_SECTION crs;
        VPN_STATUS status;
        VPN_PROFILE_T profile;
        UINT uiErrorID;

        _STATE_T() { InitializeCriticalSection(&crs); Reset(); };
        ~_STATE_T() { DeleteCriticalSection(&crs); };
        void Reset() {
            status = VPN_ST_DISCONNECTED;
            profile.Reset();
            uiErrorID = 0;
        }
        void Lock() { EnterCriticalSection(&crs); };
        void Unlock() { LeaveCriticalSection(&crs); };
    } STATE_T;

protected:
    CConfiguration *m_pConfig;
    CManagementSession *m_pManagementSession;
    CDNSLeaksPrevention m_DNSLeaks;

    HANDLE m_hThread;
    HWND m_hMainWnd;

    HANDLE m_hOpenVPNProcess;
    HANDLE m_hOpenPVNStdInRead;
    HANDLE m_hOpenPVNStdInWrite;

    HANDLE m_hStopEvent;
    HANDLE m_hCommandEvent;
    HANDLE m_hManagementDataEvent;
    HANDLE m_hCloseOpenVPNEvent;
    CString m_csCloseOpenVPNEventName;

    STATE_T m_state;

    CRITICAL_SECTION m_crsCommand;
    VPN_COMMAND m_command;

protected:
    BOOL Init();
    void Shutdown();

    DWORD Main();

    void HandleCommand();
    void HandleManagementData();

    VPN_COMMAND GetCommand();
    void SetStatus(VPN_STATUS status, UINT uiErrorID = 0);

    void UpdateStatus();
    void UpdateTrafficData();
    void RetrieveUserPassword();

    BOOL ConnectOpenVPN();
    void DisconnectOpenVPN();

    BOOL StartOpenVPNProcess();
    void StopOpenVPNProcess();

private:
    static DWORD WINAPI ThreadProc(LPVOID);

};

