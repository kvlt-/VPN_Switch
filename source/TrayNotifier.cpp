#include "stdafx.h"
#include "resource.h"
#include "TrayNotifier.h"

#define DEF_TRAY_CHECK_INTERVAL   100   // how often check if mouse left tray icon area [ms]


CTrayNotifier::CTrayNotifier()
{
    m_hMainWnd = NULL;
    m_uiTrayMessageID = 0;
    ZeroMemory(&m_trayData, sizeof(m_trayData));
    m_hThread = NULL;
    m_hStopEvent = NULL;
    m_hTrackStartEvent = NULL;
    m_bTracking = FALSE;
    ZeroMemory(&m_ptCursor, sizeof(m_ptCursor));
    InitializeCriticalSection(&m_crsTrack);
}


CTrayNotifier::~CTrayNotifier()
{
    DeleteCriticalSection(&m_crsTrack);
}

BOOL CTrayNotifier::Init(HWND hMainWnd, UINT uiTrayMessageID)
{
    BOOL bRet = FALSE;

    do {
        if (!hMainWnd || !uiTrayMessageID) break;

        m_hMainWnd = hMainWnd;
        m_uiTrayMessageID = uiTrayMessageID;

        m_trayData.cbSize = sizeof(NOTIFYICONDATA);
        m_trayData.hWnd = hMainWnd;
        m_trayData.uID = 0;
        m_trayData.uVersion = NOTIFYICON_VERSION_4;
        m_trayData.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        m_trayData.uCallbackMessage = WM_TRAY_EVENT;
        m_trayData.hIcon = LoadIcon(AfxGetResourceHandle(), MAKEINTRESOURCE(IDI_DARK));
        _tcscpy_s(m_trayData.szTip, _countof(m_trayData.szTip), DEF_APP_NAME);

        if (!Shell_NotifyIcon(NIM_ADD, &m_trayData)) {
            ZeroMemory(&m_trayData, sizeof(m_trayData));
            break;
        }

        m_bTracking = FALSE;

        m_hStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        m_hTrackStartEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        if (!m_hStopEvent || !m_hTrackStartEvent) break;

        m_hThread = CreateThread(NULL, 0, ThreadProc, this, 0, NULL);
        if (!m_hThread) break;

        bRet = TRUE;
    } while (0);

    if (!bRet) Shutdown();

    return bRet;
}


void CTrayNotifier::Shutdown()
{
    if (m_hThread) {
        SetEvent(m_hStopEvent);
        WaitForSingleObject(m_hThread, INFINITE);
    }
    CLOSEHANDLE(m_hThread);
    CLOSEHANDLE(m_hStopEvent);
    CLOSEHANDLE(m_hTrackStartEvent);
    if (m_trayData.cbSize) {
        Shell_NotifyIcon(NIM_DELETE, &m_trayData);
        ZeroMemory(&m_trayData, sizeof(m_trayData));
    }
}

BOOL CTrayNotifier::TrackMouse()
{
    BOOL bTracked;

    EnterCriticalSection(&m_crsTrack);
    GetCursorPos(&m_ptCursor);
    bTracked = m_bTracking;
    if (!m_bTracking) {
        SetEvent(m_hTrackStartEvent);
        m_bTracking = TRUE;
    }
    LeaveCriticalSection(&m_crsTrack);

    return bTracked;
}


void CTrayNotifier::Notify(VPN_STATUS status, LPCTSTR szText)
{
    if (!m_trayData.cbSize) return;

    CString csTitle;

    m_trayData.uFlags       = NIF_INFO | NIF_ICON | NIF_TIP;
    m_trayData.dwInfoFlags  = NIIF_NOSOUND | NIIF_INFO;

    switch (status)
    {
    case VPN_ST_CONNECTED:
        m_trayData.hIcon = LoadIcon(AfxGetResourceHandle(), MAKEINTRESOURCE(IDI_GREEN));
        csTitle.LoadString(IDS_INFO_CONNECTED);
        break;
    case VPN_ST_DISCONNECTED:
        m_trayData.hIcon = LoadIcon(AfxGetResourceHandle(), MAKEINTRESOURCE(IDI_DARK));
        csTitle.LoadString(IDS_INFO_DISCONNECTED);
        break;
    case VPN_ST_CONNECTING:
        m_trayData.hIcon = LoadIcon(AfxGetResourceHandle(), MAKEINTRESOURCE(IDI_ORANGE));
        csTitle.LoadString(IDS_INFO_CONNECTING);
        break;
    case VPN_ST_DISCONNECTING:
        m_trayData.hIcon = LoadIcon(AfxGetResourceHandle(), MAKEINTRESOURCE(IDI_ORANGE));
        csTitle.LoadString(IDS_INFO_DISCONNECTING);
        break;
    case VPN_ST_ERROR:
        m_trayData.dwInfoFlags = NIIF_NOSOUND | NIIF_ERROR;
        m_trayData.hIcon = LoadIcon(AfxGetResourceHandle(), MAKEINTRESOURCE(IDI_DARK));
        csTitle.LoadString(IDS_STR_ERROR);
        break;
    default:
        break;
    }

    _tcscpy_s(m_trayData.szInfoTitle, _countof(m_trayData.szInfoTitle), csTitle);
    _tcscpy_s(m_trayData.szInfo, _countof(m_trayData.szInfo), szText);
    _stprintf_s(m_trayData.szTip, _countof(m_trayData.szTip), _T("%s\r\n%s"), DEF_APP_NAME, csTitle.GetBuffer());

    Shell_NotifyIcon(NIM_MODIFY, &m_trayData);
}

void CTrayNotifier::ChangeTip(LPCTSTR szTip)
{
    if (!m_trayData.cbSize) return;

    m_trayData.uFlags = NIF_TIP;
    _tcscpy_s(m_trayData.szTip, _countof(m_trayData.szTip), szTip);

    Shell_NotifyIcon(NIM_MODIFY, &m_trayData);
}


DWORD CTrayNotifier::ThreadMain()
{
    BOOL bStop = FALSE;
    HANDLE arWait[] = {m_hTrackStartEvent, m_hStopEvent};
    DWORD dwTimeout = INFINITE;

    do {
        switch (WaitForMultipleObjects(_countof(arWait), arWait, FALSE, dwTimeout))
        {
        case WAIT_OBJECT_0:
            dwTimeout = DEF_TRAY_CHECK_INTERVAL;
            break;
        case WAIT_TIMEOUT:
            if (CheckMouseLeave()) {
                ::PostMessage(m_hMainWnd, m_uiTrayMessageID, 0, WM_MOUSELAST);
                dwTimeout = INFINITE;
            }
            break;
        case  WAIT_OBJECT_0 + 1:
        default:
            bStop = TRUE;
            break;
        }
    } while (!bStop);

    return 0;
}

BOOL CTrayNotifier::CheckMouseLeave()
{
    BOOL bLeft = FALSE;
    POINT pt;

    EnterCriticalSection(&m_crsTrack);
    GetCursorPos(&pt);
    if (pt.x != m_ptCursor.x && pt.y != m_ptCursor.y) {
        bLeft = TRUE;
        m_bTracking = FALSE;
    }
    LeaveCriticalSection(&m_crsTrack);

    return bLeft;
}


DWORD CTrayNotifier::ThreadProc(LPVOID pContext)
{
    return ((CTrayNotifier *)pContext)->ThreadMain();
}
