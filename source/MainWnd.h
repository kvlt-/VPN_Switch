
#pragma once

#include "Configuration.h"
#include "Controller.h"
#include "TrayNotifier.h"


class CMainWnd : public CWnd
{
public:
    CMainWnd();
    virtual ~CMainWnd();

    BOOL Run();

protected:
    NOTIFYICONDATA m_TrayData;
    HMENU m_hTrayMenu;
    HMENU m_hTrayProfilesMenu;
    CDialog *m_pdlgChild;
    BOOL m_bProcessing;

    CConfiguration *m_pConfig;
    CController *m_pController;
    CTrayNotifier m_TrayNotifier;

protected:
    BOOL Init();
    void Shutdown();

    BOOL StartProcessing();
    void StopProcessing();

    void ConnectToProfile(UINT uiProfile);

    void NotifyTray(VPN_STATUS status, UINT uiStringID = 0);
    void ChangeTrayTip(LPCTSTR szTip);
    void ChangeTrayTipStats();

    void DisplayTrayProfiles();
    void DisplayTrayProfilesMenu();
    void DisplayTrayMenu();
    void DisplayChildDialog(CDialog *pDlg);

    afx_msg void OnTrayCommand(UINT uiCommandID);
    afx_msg void OnTrayProfileSelected(UINT uiProfileID);
    afx_msg LRESULT OnTrayEvent(WPARAM, LPARAM);
    afx_msg LRESULT OnStatusEvent(WPARAM, LPARAM);
    afx_msg LRESULT OnByteCountEvent(WPARAM, LPARAM);

    afx_msg BOOL OnQueryEndSession();
    afx_msg void OnEndSession(BOOL bEnding);

    DECLARE_MESSAGE_MAP();

private:

};