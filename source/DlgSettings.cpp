
#include "stdafx.h"
#include "Utils.h"
#include "DlgSettings.h"


BEGIN_MESSAGE_MAP(CDlgSettings, CDialog)
    ON_MESSAGE(WM_SETTINGS_EVENT, &OnSettingsEvent)
END_MESSAGE_MAP()


CDlgSettings::CDlgSettings(CConfiguration *pConfig, CWnd *pParentWnd) : CDialog(IDD, pParentWnd)
{
    m_pConfig = pConfig;
    m_bDNS = FALSE;
    m_uiPort = 0;
}

CDlgSettings::~CDlgSettings()
{
}


void CDlgSettings::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);

    DDX_Check(pDX, IDC_SETTINGS_DNS, m_bDNS);
    DDX_Text(pDX, IDC_SETTINGS_PROFILESDIR, m_csProfilesDir);
    DDX_Text(pDX, IDC_SETTINGS_PORT, m_uiPort);
    DDV_Port(pDX, m_uiPort);
    DDX_Text(pDX, IDC_SETTINGS_PASSWORD, m_csPassword);
    DDV_Password(pDX, m_csPassword);
    DDX_Text(pDX, IDC_SETTINGS_LOGFILE, m_csLogFile);
}


void CDlgSettings::DDV_Port(CDataExchange *pDX, UINT uiPort)
{
    CEdit *ctl = (CEdit *)GetDlgItem(pDX->m_idLastControl);

    if (pDX->m_bSaveAndValidate) {
        BOOL bFail = FALSE;
        CString csText;

        if (uiPort < DEF_PORT_MIN || uiPort > DEF_PORT_MAX) {
            csText.Format(IDS_ERR_BAD_PORT, DEF_PORT_MIN, DEF_PORT_MAX);
            bFail = TRUE;
        }
        else if (!CUtils::IsPortAvailable(uiPort)) {
            csText.Format(IDS_ERR_PORT_BIND, uiPort);
            bFail = TRUE;
        }

        if (bFail) {
            DisplayWarningBox(csText);
            pDX->Fail();
        }
    }
    else {
        ctl->SetLimitText(5);
    }
}

void CDlgSettings::DDV_Password(CDataExchange *pDX, CString &csPassword)
{
    CEdit *ctl = (CEdit *)GetDlgItem(pDX->m_idLastControl);

    if (pDX->m_bSaveAndValidate) {
        size_t len = csPassword.GetLength();
        if (len  < DEF_PASSWORD_MIN || len > DEF_PASSWORD_MAX) {
            CString csText;
            csText.Format(IDS_ERR_BAD_PASSWORD, DEF_PASSWORD_MIN, DEF_PASSWORD_MAX);
            DisplayWarningBox(csText);
            pDX->Fail();
        }
    }
    else {
        ctl->SetLimitText(DEF_PASSWORD_MAX);
    }
}

BOOL CDlgSettings::OnInitDialog()
{
    m_bDNS          = m_pConfig->GetPreventDNSLeaks();
    m_pConfig->GetProfilesDir(m_csProfilesDir);
    m_uiPort        = m_pConfig->GetPort();
    m_pConfig->GetPassword(m_csPassword);
    m_pConfig->GetLogFile(m_csLogFile);

    CDialog::OnInitDialog();

    EnableControls(!m_pConfig->IsLocked());

    return FALSE;
}

void CDlgSettings::OnOK()
{
    if (!UpdateData(TRUE)) return;

    m_pConfig->SetPreventDNSLeaks(m_bDNS);
    m_pConfig->SetProfilesDir(m_csProfilesDir);
    m_pConfig->SetPort(m_uiPort);
    m_pConfig->SetPassword(m_csPassword);
    m_pConfig->SetLogFile(m_csLogFile);

    EndDialog(IDOK);
}

LRESULT CDlgSettings::OnSettingsEvent(WPARAM wParam, LPARAM lParam)
{
    EnableControls(!(BOOL)wParam);

    return 0;
}

void CDlgSettings::EnableControls(BOOL bEnable)
{
    GetDlgItem(IDC_SETTINGS_DNS)->EnableWindow(bEnable);
    GetDlgItem(IDC_SETTINGS_PROFILESDIR)->EnableWindow(bEnable);
    GetDlgItem(IDC_SETTINGS_PORT)->EnableWindow(bEnable);
    GetDlgItem(IDC_SETTINGS_PASSWORD)->EnableWindow(bEnable);
    GetDlgItem(IDC_SETTINGS_LOGFILE)->EnableWindow(bEnable);
    GetDlgItem(IDOK)->EnableWindow(bEnable);

    CString csText;
    csText.LoadString(bEnable ? IDS_INFO_SETTINGS_UNLOCKED : IDS_INFO_SETTINGS_LOCKED);
    GetDlgItem(IDC_SETTINGS_INFO)->SetWindowText(csText);
}

void CDlgSettings::DisplayWarningBox(LPCTSTR szText)
{
    CString csCaption;
    csCaption.LoadString(IDS_STR_WARNING);
    MessageBox(szText, csCaption, MB_ICONWARNING | MB_OK);
}
