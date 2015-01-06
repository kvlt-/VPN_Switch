
#include "stdafx.h"
#include "Utils.h"
#include "DlgSettings.h"

#define DEF_PORT_MIN            1025            // min OpenVPN management port
#define DEF_PORT_MAX            65535           // max OpenVPN management port
#define DEF_PASSWORD_MIN        4               // min OpenVPN management password length
#define DEF_PASSWORD_MAX        12              // max OpenVPN management password length


BEGIN_MESSAGE_MAP(CDlgSettings, CDialog)
    ON_MESSAGE(WM_SETTINGS_EVENT, &OnSettingsEvent)
END_MESSAGE_MAP()


CDlgSettings::CDlgSettings(CConfiguration *pConfig, CWnd *pParentWnd) : CDialog(IDD, pParentWnd)
{
    m_pConfig = pConfig;
}

CDlgSettings::~CDlgSettings()
{
}


void CDlgSettings::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);

    DDX_Check(pDX, IDC_SETTINGS_DNS, m_data.bPreventDNSLeaks);
    DDX_Text(pDX, IDC_SETTINGS_PROFILESDIR, m_data.csProfilesDir);
    DDX_Text(pDX, IDC_SETTINGS_PORT, m_data.uiPort);
    DDV_Port(pDX, m_data.uiPort);
    DDX_Text(pDX, IDC_SETTINGS_PASSWORD, m_data.csPassword);
    DDV_Password(pDX, m_data.csPassword);
    DDX_Text(pDX, IDC_SETTINGS_LOGFILE, m_data.csLogFile);
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
    m_pConfig->GetData(m_data);

    CDialog::OnInitDialog();

    EnableControls(!m_pConfig->IsLocked());

    return FALSE;
}

void CDlgSettings::OnOK()
{
    if (!UpdateData(TRUE)) return;

    m_pConfig->SetData(m_data);

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
