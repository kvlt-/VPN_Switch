#pragma once

#include "Configuration.h"

class CDlgSettings : public CDialog
{
    enum { IDD = IDD_SETTINGS };

public:

    CDlgSettings(CConfiguration *pConfig, CWnd *pParentWnd = NULL);
    virtual ~CDlgSettings();

    virtual void DoDataExchange(CDataExchange* pDX);

protected:
    CConfiguration *m_pConfig;

    BOOL m_bDNS;
    CString m_csProfilesDir;
    UINT m_uiPort;
    CString m_csPassword;
    CString m_csLogFile;

protected:
    virtual BOOL OnInitDialog();
    virtual void OnOK();

    DECLARE_MESSAGE_MAP()

    afx_msg LRESULT OnSettingsEvent(WPARAM wParam, LPARAM lParam);


    void AFX_API DDV_Port(CDataExchange *pDX, UINT uiPort);
    void AFX_API DDV_Password(CDataExchange *pDX, CString &csPassword);

protected:
    void EnableControls(BOOL bEnable);

    void DisplayWarningBox(LPCTSTR szText);

};