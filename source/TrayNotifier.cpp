#include "stdafx.h"
#include "TrayNotifier.h"

#define DEF_TRAY_CHECK_INTERVAL   250   // how often check if mouse left tray icon area [ms]


CTrayNotifier::CTrayNotifier()
{
    m_hTrayWnd = NULL;
    m_uiTrayMessageID = 0;
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

BOOL CTrayNotifier::Init(HWND hTrayWnd, UINT uiTrayMessageID)
{
    BOOL bRet = FALSE;

    do {
        if (!hTrayWnd || !uiTrayMessageID) break;

        m_hTrayWnd = hTrayWnd;
        m_uiTrayMessageID = uiTrayMessageID;
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
                ::PostMessage(m_hTrayWnd, m_uiTrayMessageID, 0, WM_MOUSELAST);
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
