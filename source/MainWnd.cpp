
#include "stdafx.h"
#include "Utils.h"
#include "DlgProfiles.h"
#include "DlgSettings.h"
#include "MainWnd.h"

// tray menu items message IDs
#define WM_TRAYMENU_FIRST      ID_TRAYMENU_EXIT
#define WM_TRAYMENU_LAST       ID_TRAYMENU_EDIT_PROFILES
#define WM_PROFILE_FIRST       (WM_TRAYMENU_LAST+1)
#define WM_PROFILE_LAST        (WM_TRAYMENU_FIRST+DEF_MAX_PROFILES)


CMainWnd::CMainWnd() : CWnd()
{
    ZeroMemory(&m_TrayData, sizeof(NOTIFYICONDATA));
    m_hTrayMenu             = NULL;
    m_hTrayProfilesMenu     = NULL;
    m_pConfig               = NULL;
    m_pController           = NULL;
    m_pdlgChild             = NULL;
    m_bProcessing           = FALSE;

    CreateEx(0, ::AfxRegisterWndClass(NULL), NULL, 0, 0, 0, 0, 0, NULL, NULL);
}

CMainWnd::~CMainWnd()
{
    DestroyWindow();
}

BEGIN_MESSAGE_MAP(CMainWnd, CWnd)
    ON_COMMAND_RANGE(WM_TRAYMENU_FIRST, WM_TRAYMENU_LAST, OnTrayCommand)
    ON_COMMAND_RANGE(WM_PROFILE_FIRST, WM_PROFILE_LAST, OnTrayProfileSelected)
    ON_MESSAGE(WM_TRAY_EVENT, OnTrayEvent)
    ON_MESSAGE(WM_STATUS_EVENT, OnStatusEvent)
    ON_MESSAGE(WM_BYTECOUNT_EVENT, OnByteCountEvent)
    ON_WM_QUERYENDSESSION()
    ON_WM_ENDSESSION()
    ON_WM_CLOSE()
END_MESSAGE_MAP()

BOOL CMainWnd::Run()
{
    BOOL bRet = Init();
    
    if (bRet) {
        RunModalLoop();
    }
    else {
        NotifyTray(VPN_ST_ERROR, IDS_ERR_INIT);
        Sleep(2500);
    }

    Shutdown();

    return bRet;
}

BOOL CMainWnd::Init()
{
    BOOL bRet = FALSE;

    do {
        m_pConfig = new CConfiguration();
        if (!m_pConfig) break;

        m_pController = new CController(m_pConfig, GetSafeHwnd());
        if (!m_pController) break;

        m_hTrayMenu = LoadMenu(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDR_TRAY_MENU));
        if (!m_hTrayMenu) break;

        if (!m_pConfig->Load()) break;

        if (!m_TrayNotifier.Init(GetSafeHwnd(), WM_TRAY_EVENT)) break;

        m_TrayData.cbSize = sizeof(NOTIFYICONDATA);
        m_TrayData.hWnd = GetSafeHwnd();
        m_TrayData.uID = 0;
        m_TrayData.uVersion = NOTIFYICON_VERSION_4;
        m_TrayData.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
        m_TrayData.uCallbackMessage = WM_TRAY_EVENT;
        m_TrayData.hIcon = LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_DARK));
        _tcscpy_s(m_TrayData.szTip, _countof(m_TrayData.szTip), DEF_APP_NAME);

        if (!Shell_NotifyIcon(NIM_ADD, &m_TrayData)) {
            ZeroMemory(&m_TrayData, sizeof(m_TrayData));
            break;
        }

        bRet = TRUE;
    } while (0);

    return bRet;
}

void CMainWnd::Shutdown()
{
    m_TrayNotifier.Shutdown();
    DESTROYMENU(m_hTrayMenu);
    DESTROYMENU(m_hTrayProfilesMenu);
    DESTRUCT(m_pController);
    DESTRUCT(m_pConfig);
    if (m_TrayData.cbSize) {
        Shell_NotifyIcon(NIM_DELETE, &m_TrayData);
        ZeroMemory(&m_TrayData, sizeof(m_TrayData));
    }
}

BOOL CMainWnd::StartProcessing()
{
    return m_bProcessing || (m_bProcessing = m_pController->Start());
}

void CMainWnd::StopProcessing()
{
    if (m_pdlgChild) m_pdlgChild->EndDialog(0);
    if (m_bProcessing) {
        m_pController->StopAsync();
    }
    else {
        PostQuitMessage(0);
    }
}

void CMainWnd::ConnectToProfile(UINT uiProfile)
{
    if (!StartProcessing()) {
        NotifyTray(VPN_ST_ERROR, IDS_ERR_INIT);
        return;
    }

    if (CUtils::IsProcessRunning(m_pConfig->GetOVPNExeName())) {
        NotifyTray(VPN_ST_ERROR, IDS_ERR_ALREADY_RUNNING);
        return;
    }

    m_pController->SetActiveProfile(uiProfile);
    m_pController->SetCommand(VPN_CMD_CONNECT);
}


void CMainWnd::OnTrayCommand(UINT uiCommandID)
{
    switch (uiCommandID)
    {
    case ID_TRAYMENU_EXIT:
        StopProcessing();
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
        if (!m_TrayNotifier.TrackMouse()) {
            if (m_pController->GetStatus() == VPN_ST_CONNECTED) {
                ChangeTrayTipStats();
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

    switch (status)
    {
    case VPN_ST_CONNECTING:
        m_pConfig->LockSettings(TRUE);
        if (m_pdlgChild) ::PostMessage(m_pdlgChild->GetSafeHwnd(), WM_SETTINGS_EVENT, TRUE, 0);
        break;
    case VPN_ST_DISCONNECTED:
    case VPN_ST_ERROR:
        m_pConfig->LockSettings(FALSE);
        if (m_pdlgChild) ::PostMessage(m_pdlgChild->GetSafeHwnd(), WM_SETTINGS_EVENT, FALSE, 0);
        break;
    case VPN_ST_EXITED:
        m_pController->WaitForStop();
        PostQuitMessage(0);
        break;
    default:
        break;
    }

    NotifyTray(status, uiErrorID);

    return 0;
}
LRESULT CMainWnd::OnByteCountEvent(WPARAM wParam, LPARAM lParam)
{


    return 0;
}

BOOL CMainWnd::OnQueryEndSession()
{
    return TRUE;
}

void CMainWnd::OnEndSession(BOOL bEnding)
{
    StopProcessing();
}

void CMainWnd::OnClose()
{
    StopProcessing();
}

void CMainWnd::DisplayTrayProfiles()
{
    UINT uProfileCount      = 0;
    VPN_STATUS status       = m_pController->GetStatus();
   
    switch (status) {
    case VPN_ST_CONNECTED:
        m_pController->SetCommand(VPN_CMD_DISCONNECT);
        break;
    case VPN_ST_ERROR:
    case VPN_ST_DISCONNECTED:
        uProfileCount = m_pConfig->ReloadProfiles();
        if (uProfileCount == 0) {
            NotifyTray(VPN_ST_ERROR, IDS_ERR_NO_PROFILES);
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


void CMainWnd::NotifyTray(VPN_STATUS status, UINT uiStringID)
{
    if (!m_TrayData.cbSize) return;

    CString csTitle, csText, csProfileName, csIP;

    m_TrayData.uFlags       = NIF_INFO|NIF_ICON|NIF_TIP;
    m_TrayData.dwInfoFlags  = NIIF_NOSOUND|NIIF_INFO;

    if (status != VPN_ST_ERROR) m_pController->GetActiveProfileName(csProfileName);

    switch (status)
    {
    case VPN_ST_CONNECTED:
        m_TrayData.hIcon = LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_GREEN));
        csTitle.LoadString(IDS_INFO_CONNECTED);
        m_pController->GetExternalIP(csIP);
        csText.Format(IDS_INFO_CONNECTED_TEXT, csProfileName, csIP);
        break;
    case VPN_ST_DISCONNECTED:
        m_TrayData.hIcon = LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_DARK));
        csTitle.LoadString(IDS_INFO_DISCONNECTED);
        csText.Format(IDS_INFO_DISCONNECTED_TEXT, csProfileName);
        break;
    case VPN_ST_CONNECTING:
        m_TrayData.hIcon = LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_ORANGE));
        csTitle.LoadString(IDS_INFO_CONNECTING);
        csText.Format(IDS_INFO_CONNECTING_TEXT, csProfileName);
        break;
    case VPN_ST_DISCONNECTING:
        m_TrayData.hIcon = LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_ORANGE));
        csTitle.LoadString(IDS_INFO_DISCONNECTING);
        csText.Format(IDS_INFO_DISCONNECTING_TEXT, csProfileName);
        break;
    case VPN_ST_ERROR:
        m_TrayData.dwInfoFlags = NIIF_NOSOUND|NIIF_ERROR;
        m_TrayData.hIcon = LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_DARK));
        csTitle.LoadString(IDS_STR_ERROR);
        csText.LoadString(uiStringID ? uiStringID : IDS_ERR_UNSPECIFIED);
        break;
    default: break;
    }

    _tcscpy_s(m_TrayData.szInfoTitle, _countof(m_TrayData.szInfoTitle), csTitle);
    _tcscpy_s(m_TrayData.szInfo, _countof(m_TrayData.szInfo), csText);
    _stprintf_s(m_TrayData.szTip, _countof(m_TrayData.szTip), _T("%s\r\n%s"), DEF_APP_NAME, csTitle);

    Shell_NotifyIcon(NIM_MODIFY, &m_TrayData);
}

void CMainWnd::ChangeTrayTip(LPCTSTR szTip)
{
    if (!m_TrayData.cbSize) return;

    m_TrayData.uFlags = NIF_TIP;
    _tcscpy_s(m_TrayData.szTip, _countof(m_TrayData.szTip), szTip);

    Shell_NotifyIcon(NIM_MODIFY, &m_TrayData);
}

void CMainWnd::ChangeTrayTipStats()
{
    CController::TRAFFIC_DATA_T data;
    m_pController->GetTrafficData(data);

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

    CString csIP;
    m_pController->GetExternalIP(csIP);

    CString csLabelIP, csLabelUptime;
    csLabelIP.LoadString(IDS_STR_EXTERNAL_IP);
    csLabelUptime.LoadString(IDS_STR_UPTIME);

    CString csTip;
    csTip.Format(_T("%s\r\nD:  %-10s (%s)\r\nU:  %-10s (%s)\r\n%s: %s\r\n%s: %s"),
        DEF_APP_NAME, szSpeedDownload, szTotalDownload, szSpeedUpload, szTotalUpload, csLabelUptime, szUptime, csLabelIP, csIP);

    ChangeTrayTip(csTip);
}