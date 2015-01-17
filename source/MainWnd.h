
#pragma once

#include "Configuration.h"
#include "Controller.h"
#include "TrayNotifier.h"


class CMainWnd : public CWnd
{
public:
    CMainWnd();
    virtual ~CMainWnd();

    BOOL Create();

protected:
    HMENU m_hTrayMenu;
    HMENU m_hTrayProfilesMenu;
    CDialog *m_pdlgChild;
    BOOL m_bProcessing;

    CConfiguration *m_pConfig;
    CController *m_pController;
    CTrayNotifier m_trayNotifier;

protected:
    BOOL StartProcessing();             // called on first connect request
    void StopProcessing(BOOL bWait);    // called on exit request

    void ConnectToProfile(UINT uiProfile);
    void DisplayTrayProfiles();
    void DisplayTrayProfilesMenu();
    void DisplayTrayMenu();
    void DisplayChildDialog(CDialog *pDlg);

    CString ConstructNotifyText(VPN_STATUS status, UINT uiStringID = 0);
    CString ConstructTrafficStatsTip();

protected:
    DECLARE_DYNAMIC(CMainWnd);
    DECLARE_MESSAGE_MAP();

    afx_msg void OnTrayCommand(UINT uiCommandID);
    afx_msg void OnTrayProfileSelected(UINT uiProfileID);
    afx_msg LRESULT OnTrayEvent(WPARAM, LPARAM);
    afx_msg LRESULT OnStatusEvent(WPARAM, LPARAM);
    afx_msg LRESULT OnByteCountEvent(WPARAM, LPARAM);

    afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
    afx_msg void OnDestroy();
    afx_msg void OnEndSession(BOOL bEnding);
    afx_msg void OnClose();

private:

};