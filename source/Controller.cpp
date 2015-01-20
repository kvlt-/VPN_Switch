
#include "stdafx.h"
#include "resource.h"
#include "Utils.h"
#include "Controller.h"

#define DEF_OPENVPN_EVENT_NAME  DEF_APP_NAME _T("_ExitEvent")


CController::CController(CConfiguration *pConfig, HWND hMainWnd)
{
    m_hMainWnd              = hMainWnd;
    m_pConfig               = pConfig;
    m_hThread               = NULL;
    m_pManagementSession    = NULL;

    m_hStopEvent            = NULL;
    m_hCommandEvent         = NULL;
    m_hManagementDataEvent  = NULL;
    m_hCloseOpenVPNEvent    = NULL;

    m_hOpenVPNProcess       = NULL;
    m_hOpenPVNStdInRead     = NULL;
    m_hOpenPVNStdInWrite    = NULL;

    m_command               = VPN_CMD_NONE;
    InitializeCriticalSection(&m_crsCommand);
}

CController::~CController()
{
    DeleteCriticalSection(&m_crsCommand);
}

BOOL CController::Init()
{
    BOOL bRet = FALSE;

    do {
        if (!m_pConfig) break;

        m_pManagementSession = new CManagementSession();
        if (!m_pManagementSession) break;

        m_hStopEvent            = CreateEvent(NULL, FALSE, FALSE, NULL);
        m_hCommandEvent         = CreateEvent(NULL, FALSE, FALSE, NULL);
        m_hManagementDataEvent  = CreateEvent(NULL, FALSE, FALSE, NULL);
        m_hCloseOpenVPNEvent    = CUtils::GetUniqueSessionEvent(DEF_OPENVPN_EVENT_NAME, m_csCloseOpenVPNEventName);
        if (!m_hStopEvent || !m_hCommandEvent || !m_hManagementDataEvent || !m_hCloseOpenVPNEvent) break;

        SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
        if (!CreatePipe(&m_hOpenPVNStdInRead, &m_hOpenPVNStdInWrite, &sa, 0)) break;
        if (!SetHandleInformation(m_hOpenPVNStdInWrite, HANDLE_FLAG_INHERIT, 0)) break;

        m_state.Reset();

        bRet = TRUE;
    } while (0);

    return bRet;
}

void CController::Shutdown()
{
    CLOSEHANDLE(m_hOpenPVNStdInRead);
    CLOSEHANDLE(m_hOpenPVNStdInWrite);
    CLOSEHANDLE(m_hStopEvent);
    CLOSEHANDLE(m_hCommandEvent);
    CLOSEHANDLE(m_hManagementDataEvent);
    CLOSEHANDLE(m_hCloseOpenVPNEvent);
    DESTRUCT(m_pManagementSession);
}

BOOL CController::Start()
{
    BOOL bRet = Init();

    if (bRet) {
        m_hThread = CreateThread(NULL, 0, ThreadProc, this, 0, NULL);
        bRet = m_hThread != NULL;
    }
    if (!bRet) {
        Shutdown();
    }

    return bRet;
}

void CController::Stop(BOOL bExiting)
{
    if (m_hStopEvent) SetEvent(m_hStopEvent);
    if (bExiting) {
        if (m_hThread) {
            if (WaitForSingleObject(m_hThread, DEF_SHUTDOWN_TIMEOUT * 1000) == WAIT_TIMEOUT) {
                TerminateThread(m_hThread, 0);
            }
            CLOSEHANDLE(m_hThread);
        }
        Shutdown();
    }
}

void CController::WaitForStop()
{
    if (m_hThread) {
        WaitForSingleObject(m_hThread, INFINITE);
        CLOSEHANDLE(m_hThread);
    }
    Shutdown();
}

DWORD CController::Main()
{
    BOOL bStop              = FALSE;
    DWORD dwWaitObject      = 0;
    HANDLE arWait[] = {m_hCommandEvent, m_hManagementDataEvent, m_hStopEvent};

    do {
        dwWaitObject = WaitForMultipleObjects(_countof(arWait), arWait, FALSE, INFINITE);

        switch (dwWaitObject)
        {
        case WAIT_OBJECT_0:
            HandleCommand();
            break;
        case WAIT_OBJECT_0 + 1:
            HandleManagementData();
            break;
        case WAIT_OBJECT_0 + 2:
        default:
            bStop = TRUE;
            break;
        }
    } while (!bStop);

    DisconnectOpenVPN();
    SetStatus(VPN_ST_EXITED);

    return 0;
}

void CController::HandleCommand()
{
    switch (GetCommand())
    {
    case VPN_CMD_CONNECT:
    {
        ConnectOpenVPN();
        break;
    }
    case VPN_CMD_DISCONNECT:
    {
        DisconnectOpenVPN();
        break;
    }
    default: break;
    }
}

void CController::HandleManagementData()
{
    DWORD dwEvents = m_pManagementSession->GetEvents();

    if (dwEvents & CManagementSession::EVENT_STATUS) {
        UpdateStatus();
    }
    if (dwEvents & CManagementSession::EVENT_BYTECOUNT) {
        UpdateTrafficData();
    }
}

VPN_STATUS CController::GetStatus(PUINT puiErrorID)
{
    VPN_STATUS status;

    m_state.Lock();
    status = m_state.status;
    if (puiErrorID) {
        *puiErrorID = m_state.uiErrorID;
        m_state.uiErrorID = 0;
    }
    m_state.Unlock();

    return status;
}

void CController::SetStatus(VPN_STATUS status, UINT uiErrorID)
{
    m_state.Lock();
    m_state.status = status;
    if (status == VPN_ST_ERROR) {
        if (uiErrorID) m_state.uiErrorID = uiErrorID;
    }
    m_state.Unlock();

    if (m_hMainWnd) ::PostMessage(m_hMainWnd, WM_STATUS_EVENT, NULL, NULL);
}

void CController::SetActiveProfile(UINT uiProfile)
{
    m_state.Lock();
    if (!m_pConfig->GetProfile(uiProfile, m_state.profile)) m_state.profile.Reset();
    m_state.Unlock();

}
void CController::GetActiveProfileName(CString &csProfileName)
{
    m_state.Lock();
    csProfileName = m_state.profile.csName;
    m_state.Unlock();
}

void CController::GetActiveProfileConfPath(CString &csConfPath)
{
    m_state.Lock();
    csConfPath = m_state.profile.csConfPath;
    m_state.Unlock();

}

void CController::GetExternalIP(CString &csExternalIP)
{
    if (m_pManagementSession) m_pManagementSession->GetExternalIP(csExternalIP);
}

void CController::GetTrafficData(CController::TRAFFIC_DATA_T &data)
{
    CManagementSession::BYTECOUNTS_T bcs;
    FILETIME ftConnectTime;
    m_pManagementSession->GetByteCounts(&bcs, &ftConnectTime);

    UINT uiIndex = bcs.uiIndex > 0 ? bcs.uiIndex - 1 : _countof(bcs.data) - 1;
    CManagementSession::BYTECOUNT_T *pbcLast = &bcs.data[uiIndex];
    CManagementSession::BYTECOUNT_T *pbcFirst = NULL;
    do {
        uiIndex  = uiIndex > 0 ? uiIndex - 1 : _countof(bcs.data) - 1;
        if (((PLARGE_INTEGER)&bcs.data[uiIndex].ftTimestamp)->QuadPart == 0) break;

        pbcFirst = &bcs.data[uiIndex];
    } while (uiIndex != bcs.uiIndex);

    ULONGLONG ullTimeDiff;
    ULONGLONG ullBytesInDiff;
    ULONGLONG ullBytesOutDiff;
    if (pbcFirst) {
        ullTimeDiff     = CUtils::FiletimeDiff(&pbcFirst->ftTimestamp, &pbcLast->ftTimestamp);
        ullBytesInDiff  = pbcLast->ullBytesIn - pbcFirst->ullBytesIn;
        ullBytesOutDiff = pbcLast->ullBytesOut - pbcFirst->ullBytesOut;
    }
    else {
        ullTimeDiff     = CUtils::FiletimeDiff(&pbcFirst->ftTimestamp, &ftConnectTime);
        ullBytesInDiff  = pbcLast->ullBytesIn;
        ullBytesOutDiff = pbcLast->ullBytesOut;
    }

    FILETIME ftTime;
    GetSystemTimeAsFileTime(&ftTime);

    data.ullTotalTime   = CUtils::FiletimeDiff(&ftTime, &ftConnectTime);
    data.ullTotalInKb   = pbcLast->ullBytesIn >> 10;
    data.ullTotalOutKb  = pbcLast->ullBytesOut >> 10;
    data.dwSpeedInKbs   = (DWORD)((ullBytesInDiff >> 10) / ullTimeDiff);
    data.dwSpeedOutKbs  = (DWORD)((ullBytesOutDiff >> 10) / ullTimeDiff);
}

VPN_COMMAND CController::GetCommand()
{
    VPN_COMMAND command;

    EnterCriticalSection(&m_crsCommand);
    command = m_command;
    LeaveCriticalSection(&m_crsCommand);

    return command;
}

void CController::SetCommand(VPN_COMMAND command)
{
    EnterCriticalSection(&m_crsCommand);
    m_command = command;
    LeaveCriticalSection(&m_crsCommand);

    SetEvent(m_hCommandEvent);
}

void CController::UpdateStatus()
{
    VPN_STATUS status = m_pManagementSession->GetStatus();

    if (status == VPN_ST_ERROR) {
        SetStatus(status, IDS_ERR_OPENVPN);
        DisconnectOpenVPN();
    }
    else {
        if (status == VPN_ST_CONNECTED) {
            if (m_pConfig->GetPreventDNSLeaks()) {
                m_DNSLeaks.Prevent(TRUE);
            }
        }
        else {
            VPN_STATUS prevStatus = GetStatus();
            if (prevStatus == VPN_ST_CONNECTED) {
                if (m_pConfig->GetPreventDNSLeaks()) {
                    m_DNSLeaks.Prevent(FALSE);
                }
            }
            else if (status == VPN_ST_DISCONNECTED && prevStatus == VPN_ST_ERROR)
                return;
        }
        SetStatus(status);
    }
}

void CController::UpdateTrafficData()
{
    if (GetStatus() == VPN_ST_CONNECTED && m_hMainWnd) ::PostMessage(m_hMainWnd, WM_BYTECOUNT_EVENT, NULL, NULL);
}

BOOL CController::ConnectOpenVPN()
{
    BOOL bRet = TRUE;
    CManagementSession::ARGS_T args;

    do {
        if (GetStatus() == VPN_ST_CONNECTED) break;

        SetStatus(VPN_ST_CONNECTING);

        bRet = StartOpenVPNProcess();
        if (!bRet) break;

        args.uiPort             = m_pConfig->GetPort();
        args.uiBytecountInterval = m_pConfig->GetBytecountInterval();
        args.csPasswordA        = CT2A(m_pConfig->GetPassword());
        args.hDataReadyEvent    = m_hManagementDataEvent;
        args.hSessionReadyEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        if (!args.hSessionReadyEvent) { bRet =FALSE; break; }

        bRet = m_pManagementSession->Start(args);
        if (!bRet) break;

        bRet = WaitForSingleObject(args.hSessionReadyEvent, DEF_OPENVPN_TIMEOUT * 1000) == WAIT_OBJECT_0;
        if (!bRet) m_pManagementSession->Stop(TRUE);
    } while (0);

    if (!bRet) SetStatus(VPN_ST_ERROR, IDS_ERR_OPENVPN_INIT_FAILED);

    CLOSEHANDLE(args.hSessionReadyEvent);

    return bRet;
}

void CController::DisconnectOpenVPN()
{
    if (GetStatus() == VPN_ST_CONNECTED) SetStatus(VPN_ST_DISCONNECTING);

    if (m_pConfig->GetPreventDNSLeaks()) m_DNSLeaks.Prevent(FALSE);
    StopOpenVPNProcess();   // attempt graceful exit via global event
    if (m_pManagementSession) m_pManagementSession->Stop(); // possible SIGTERM close
}

BOOL CController::StartOpenVPNProcess()
{
    BOOL bRet = TRUE;

    do {
        UINT uiPort = m_pConfig->GetPort();
        if (!CUtils::IsPortAvailable(uiPort)) { bRet = FALSE; break; }

        CString csConfPath;
        GetActiveProfileConfPath(csConfPath);

        // http://openvpn.net/index.php/open-source/documentation/manuals/65-openvpn-20x-manpage.html
        CString csCommandLine;
        csCommandLine.Format(
            _T("%s --config \"%s\" --service %s 0 --log \"%s\" --suppress-timestamps --auth-retry nointeract --management 127.0.0.1 %u stdin --management-query-passwords --management-hold"),
            m_pConfig->GetOVPNExeName(),
            csConfPath,
            m_csCloseOpenVPNEventName,
            m_pConfig->GetLogFile(),
            uiPort
            );

        STARTUPINFO startupInfo = {0};
        startupInfo.cb          = sizeof(STARTUPINFO);
        startupInfo.hStdInput   = m_hOpenPVNStdInRead;
        startupInfo.hStdOutput  = GetStdHandle(STD_OUTPUT_HANDLE);
        startupInfo.hStdError   = GetStdHandle(STD_ERROR_HANDLE);
        startupInfo.dwFlags     = STARTF_USESTDHANDLES;

        ResetEvent(m_hCloseOpenVPNEvent);

        PROCESS_INFORMATION processInfo = {0};
        bRet = CreateProcess(m_pConfig->GetOVPNExePath(), csCommandLine.GetBuffer(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, m_pConfig->GetOVPNBinDir(), &startupInfo, &processInfo);
        if (!bRet) break;

        m_hOpenVPNProcess = processInfo.hProcess;
        CLOSEHANDLE(processInfo.hThread);

        CT2A szPassword(m_pConfig->GetPassword());
        DWORD dwBytes;
        bRet = WriteFile(m_hOpenPVNStdInWrite, szPassword, strlen(szPassword) + 1, &dwBytes, NULL);
        if (!bRet) break;
    } while (0);

    if (!bRet) StopOpenVPNProcess();

    return bRet;
}

void CController::StopOpenVPNProcess()
{
    if (m_hOpenVPNProcess) {
        SetEvent(m_hCloseOpenVPNEvent);
        WaitForSingleObject(m_hOpenVPNProcess, DEF_OPENVPN_TIMEOUT * 1000);
        CLOSEHANDLE(m_hOpenVPNProcess);
    }
}

DWORD CController::ThreadProc(LPVOID pContext)
{
    return ((CController *)pContext)->Main();
}
