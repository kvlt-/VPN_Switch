
#include "stdafx.h"
#include "Utils.h"
#include "DlgProfiles.h"
#include "DlgSettings.h"
#include "DlgAuth.h"
#include "MainWnd.h"

// tray menu items message IDs
#define WM_TRAYMENU_FIRST      ID_TRAYMENU_EXIT
#define WM_TRAYMENU_LAST       ID_TRAYMENU_EDIT_PROFILES
#define WM_PROFILE_FIRST       (WM_TRAYMENU_LAST+1)
#define WM_PROFILE_LAST        (WM_TRAYMENU_FIRST+DEF_MAX_PROFILES)


IMPLEMENT_DYNAMIC(CMainWnd, CWnd)

CMainWnd::CMainWnd() : CWnd()
{
    m_hTrayMenu             = NULL;
    m_hTrayProfilesMenu     = NULL;
    m_pdlgChild             = NULL;
    m_bProcessing           = FALSE;

    m_pConfig               = NULL;
    m_pController           = NULL;
}

CMainWnd::~CMainWnd()
{
}

BOOL CMainWnd::Create()
{
    LPCTSTR szWndClass = AfxRegisterWndClass(0, NULL, NULL, LoadIcon(AfxGetResourceHandle(), MAKEINTRESOURCE(IDI_MAIN)));

    return CreateEx(0, szWndClass, NULL, 0, 0, 0, 0, 0, NULL, NULL);
}

BOOL CMainWnd::StartProcessing()
{
    return m_bProcessing || (m_bProcessing = m_pController->Start());
}

void CMainWnd::StopProcessing(BOOL bWait)
{
    if (m_pdlgChild) {
        m_pdlgChild->EndDialog(0);
    }
    if (m_bProcessing) {
        m_pController->Stop(bWait);
        if (bWait) DestroyWindow();
    }
    else {
        DestroyWindow();
    }
}



BEGIN_MESSAGE_MAP(CMainWnd, CWnd)
    ON_WM_CREATE()
    ON_WM_DESTROY()
    ON_WM_ENDSESSION()
    ON_WM_CLOSE()
    ON_COMMAND_RANGE(WM_TRAYMENU_FIRST, WM_TRAYMENU_LAST, OnTrayCommand)
    ON_COMMAND_RANGE(WM_PROFILE_FIRST, WM_PROFILE_LAST, OnTrayProfileSelected)
    ON_MESSAGE(WM_TRAY_EVENT, OnTrayEvent)
    ON_MESSAGE(WM_STATUS_EVENT, OnStatusEvent)
    ON_MESSAGE(WM_BYTECOUNT_EVENT, OnByteCountEvent)
    ON_MESSAGE(WM_AUTHREQUEST_EVENT, OnAuthRequestEvent)
END_MESSAGE_MAP()

int CMainWnd::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
    BOOL bInit = FALSE;

    do {
        // tray notifier first to report possible init error
        if (!m_trayNotifier.Init(GetSafeHwnd(), WM_TRAY_EVENT)) break;

        m_pConfig = new CConfiguration();
        if (!m_pConfig) break;

        m_pController = new CController(m_pConfig, GetSafeHwnd());
        if (!m_pController) break;

        m_hTrayMenu = LoadMenu(AfxGetResourceHandle(), MAKEINTRESOURCE(IDR_TRAY_MENU));
        if (!m_hTrayMenu) break;

        if (!m_pConfig->Load()) break;

        bInit = TRUE;
    } while (0);

    if (!bInit) {
        m_trayNotifier.Notify(VPN_ST_ERROR, ConstructNotifyText(VPN_ST_ERROR, IDS_ERR_INIT));
        Sleep(2000);
        return -1;
    }
    
    return CWnd::OnCreate(lpCreateStruct);
}

void CMainWnd::OnDestroy()
{
    m_trayNotifier.Shutdown();
    DESTROYMENU(m_hTrayMenu);
    DESTROYMENU(m_hTrayProfilesMenu);
    DESTRUCT(m_pController);
    DESTRUCT(m_pConfig);

    CWnd::OnDestroy();
}

void CMainWnd::OnEndSession(BOOL bEnding)
{
    StopProcessing(TRUE);
}

void CMainWnd::OnClose()
{
    StopProcessing(TRUE);
}

void CMainWnd::OnTrayCommand(UINT uiCommandID)
{
    switch (uiCommandID)
    {
    case ID_TRAYMENU_EXIT:
        StopProcessing(FALSE);
        break;
    case ID_TRAYMENU_SETTINGS:
    {
        CDlgSettings dlg(m_pConfig, this);
        DisplayChildDialog(&dlg);
        break;
    }
    case ID_TRAYMENU_SHOW_LOG:
        ShellExecute(GetSafeHwnd(), _T("open"), m_pConfig->GetLogFile(), NULL, NULL, SW_SHOW);
        break;
    case ID_TRAYMENU_EDIT_PROFILES:
    {
        CDlgProfiles dlg(m_pConfig, this);
        DisplayChildDialog(&dlg);
        break;
    }
    default:
        break;
    }
}

void CMainWnd::OnTrayProfileSelected(UINT uiProfileID)
{
    ConnectToProfile(uiProfileID - WM_PROFILE_FIRST);
}

LRESULT CMainWnd::OnTrayEvent(WPARAM wParam, LPARAM lParam)
{
    switch ((UINT)lParam)
    {
    case WM_LBUTTONUP:
        DisplayTrayProfiles();
        break;
    case WM_RBUTTONUP:
        DisplayTrayMenu();
        break;
    case WM_MOUSEMOVE:
        // check if mouse has just entered tray icon area
        if (!m_trayNotifier.TrackMouse()) {
            if (m_pController->GetStatus() == VPN_ST_CONNECTED) {
                m_trayNotifier.ChangeTip(ConstructTrafficStatsTip());
            }
        }
        break;
    case WM_MOUSELAST:
        // mouse left tray icon area
        break;
    default:
        break;
    }

    return 0;
}

LRESULT CMainWnd::OnStatusEvent(WPARAM wParam, LPARAM lParam)
{
    UINT uiErrorID;
    VPN_STATUS status = m_pController->GetStatus(&uiErrorID);
    BOOL bNotify = TRUE;
    
    switch (status)
    {
    case VPN_ST_CONNECTING:
        bNotify = FALSE;
        m_pConfig->LockSettings(TRUE);
        if (m_pdlgChild) ::PostMessage(m_pdlgChild->GetSafeHwnd(), WM_SETTINGS_EVENT, TRUE, 0);
        break;
    case VPN_ST_DISCONNECTING:
        bNotify = FALSE;
        break;
    case VPN_ST_DISCONNECTED:
    case VPN_ST_ERROR:
        m_pConfig->LockSettings(FALSE);
        if (m_pdlgChild) ::PostMessage(m_pdlgChild->GetSafeHwnd(), WM_SETTINGS_EVENT, FALSE, 0);
        break;
    case VPN_ST_EXITED:     // async stop confirmation
        m_pController->WaitForStop();
        DestroyWindow();
        break;
    default:
        break;
    }

    if (bNotify) m_trayNotifier.Notify(status, ConstructNotifyText(status, uiErrorID));

    return 0;
}

LRESULT CMainWnd::OnByteCountEvent(WPARAM wParam, LPARAM lParam)
{
    // this is empty, byte count is on request only

    return 0;
}

LRESULT CMainWnd::OnAuthRequestEvent(WPARAM wParam, LPARAM lParam)
{
    CDlgAuth dlgAuth(m_pController->GetActiveProfileName(), this);
    if (dlgAuth.DoModal() == IDOK)
        m_pController->SetAuthInfo(dlgAuth.GetAuthUser(), dlgAuth.GetAuthPassword());
    else
        m_pController->SetAuthCancel();

    return 0;
}

void CMainWnd::ConnectToProfile(UINT uiProfile)
{
    if (!StartProcessing()) {
        m_trayNotifier.Notify(VPN_ST_ERROR, ConstructNotifyText(VPN_ST_ERROR, IDS_ERR_INIT));
        return;
    }

    if (CUtils::IsProcessRunning(m_pConfig->GetOVPNExeName())) {
        m_trayNotifier.Notify(VPN_ST_ERROR, ConstructNotifyText(VPN_ST_ERROR, IDS_ERR_ALREADY_RUNNING));
        return;
    }

    m_pController->SetActiveProfile(uiProfile);
    m_pController->SetCommand(VPN_CMD_CONNECT);
}

void CMainWnd::DisplayTrayProfiles()
{
    UINT uProfileCount      = 0;
    VPN_STATUS status       = m_pController->GetStatus();
   
    switch (status) {
    case VPN_ST_CONNECTED:
    case VPN_ST_CONNECTING:
        m_pController->SetCommand(VPN_CMD_DISCONNECT);
        break;
    case VPN_ST_ERROR:
    case VPN_ST_DISCONNECTED:
        uProfileCount = m_pConfig->ReloadProfiles();
        if (uProfileCount == 0) {
            m_trayNotifier.Notify(VPN_ST_ERROR, ConstructNotifyText(VPN_ST_ERROR, IDS_ERR_NO_PROFILES));
        }
        else if (uProfileCount == 1) {
            ConnectToProfile(0);
        }
        else {
            DisplayTrayProfilesMenu();
        }
    default: break;
    }

}

void CMainWnd::DisplayTrayProfilesMenu()
{
    UINT uIndex             = 0;
    MENUITEMINFO menuItem   = { 0 };
    CPoint pt;
    VPN_PROFILE_T profile;
    TCHAR szName[MAX_PATH];
    
    DESTROYMENU(m_hTrayProfilesMenu);

    m_hTrayProfilesMenu = CreatePopupMenu();

    ZeroMemory(&menuItem, sizeof(MENUITEMINFO));
    menuItem.cbSize = sizeof(MENUITEMINFO);
    menuItem.fMask = MIIM_STRING | MIIM_ID;
    menuItem.dwTypeData = szName;

    while (m_pConfig->GetProfile(uIndex, profile)) {
        _tcscpy_s(szName, _countof(szName), profile.csName);
        menuItem.cch = _tcslen(szName);
        menuItem.wID = WM_PROFILE_FIRST+uIndex;
        InsertMenuItem(m_hTrayProfilesMenu, uIndex, TRUE, &menuItem);
        uIndex++;
    }

    GetCursorPos(&pt);
    SetForegroundWindow();
    TrackPopupMenu(m_hTrayProfilesMenu, TPM_RIGHTALIGN|TPM_BOTTOMALIGN|TPM_LEFTBUTTON, pt.x, pt.y, 0, GetSafeHwnd(), NULL);
    PostMessage(WM_NULL, 0, 0);
}

void CMainWnd::DisplayTrayMenu()
{
    CPoint pt;
    GetCursorPos(&pt);
    SetForegroundWindow();
    TrackPopupMenu(GetSubMenu(m_hTrayMenu, 0), TPM_RIGHTALIGN | TPM_BOTTOMALIGN | TPM_LEFTBUTTON, pt.x, pt.y, 0, GetSafeHwnd(), NULL);
    PostMessage(WM_NULL, 0, 0);
}

void CMainWnd::DisplayChildDialog(CDialog *pDlg)
{
    if (m_pdlgChild) {
        ::BringWindowToTop(m_pdlgChild->GetSafeHwnd());
    }
    else {
        m_pdlgChild = pDlg;
        pDlg->DoModal();
        m_pdlgChild = NULL;
    }
}

CString CMainWnd::ConstructTrafficStatsTip()
{
    CController::TRAFFIC_DATA_T data = m_pController->GetTrafficData();

    TCHAR szSpeedDownload[64];
    TCHAR szSpeedUpload[64];
    CUtils::FormatSpeedKbs(data.dwSpeedInKbs, szSpeedDownload, _countof(szSpeedDownload));
    CUtils::FormatSpeedKbs(data.dwSpeedOutKbs, szSpeedUpload, _countof(szSpeedUpload));

    TCHAR szTotalDownload[64];
    TCHAR szTotalUpload[64];
    CUtils::FormatSizeKb(data.ullTotalInKb, szTotalDownload, _countof(szTotalDownload));
    CUtils::FormatSizeKb(data.ullTotalOutKb, szTotalUpload, _countof(szTotalUpload));

    TCHAR szUptime[64];
    CUtils::FormatTimeElapsed(data.ullTotalTime, szUptime, _countof(szUptime));

    CString csIP = m_pController->GetExternalIP();

    CString csLabelIP, csLabelUptime;
    csLabelIP.LoadString(IDS_STR_EXTERNAL_IP);
    csLabelUptime.LoadString(IDS_STR_UPTIME);

    CString csTip;
    csTip.Format(_T("%s\r\nD:  %-10s (%s)\r\nU:  %-10s (%s)\r\n%s: %s\r\n%s: %s"),
        DEF_APP_NAME, szSpeedDownload, szTotalDownload, szSpeedUpload, szTotalUpload, csLabelUptime, szUptime, csLabelIP, csIP);

    return csTip;
}

CString CMainWnd::ConstructNotifyText(VPN_STATUS status, UINT uiStringID)
{
    CString csText, csProfileName, csIP;

    switch (status)
    {
    case VPN_ST_CONNECTED:
        csProfileName = m_pController->GetActiveProfileName();
        csIP = m_pController->GetExternalIP();
        csText.Format(IDS_INFO_CONNECTED_TEXT, csProfileName, csIP);
        break;
    case VPN_ST_DISCONNECTED:
        csProfileName = m_pController->GetActiveProfileName();
        csText.Format(IDS_INFO_DISCONNECTED_TEXT, csProfileName);
        break;
    case VPN_ST_CONNECTING:
        csProfileName = m_pController->GetActiveProfileName();
        csText.Format(IDS_INFO_CONNECTING_TEXT, csProfileName);
        break;
    case VPN_ST_DISCONNECTING:
        csProfileName = m_pController->GetActiveProfileName();
        csText.Format(IDS_INFO_DISCONNECTING_TEXT, csProfileName);
        break;
    case VPN_ST_ERROR:
        csText.LoadString(uiStringID ? uiStringID : IDS_ERR_UNSPECIFIED);
        break;
    default:
        break;
    }

    return csText;
}