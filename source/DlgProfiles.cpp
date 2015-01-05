
#include "stdafx.h"
#include "DlgProfiles.h"

#define DEF_MAX_PROFILE_LENGTH      16384

BEGIN_MESSAGE_MAP(CDlgProfiles, CDialog)
    ON_BN_CLICKED(IDC_PROFILES_ADD,         &OnBtnAddClicked)
    ON_BN_CLICKED(IDC_PROFILES_REMOVE,      &OnBtnRemoveClicked)
    ON_BN_CLICKED(IDC_PROFILES_EDITSAVE,    &OnBtnEditSaveClicked)
    ON_LBN_SELCHANGE(IDC_PROFILESLIST,      &OnProfileListSelChange)
END_MESSAGE_MAP()


CDlgProfiles::CDlgProfiles(CConfiguration *pConfig, CWnd *pParentWnd) : CDialog(IDD, pParentWnd)
{
    m_pConfig = pConfig;
    m_bEditing = FALSE;
    m_iSelectedProfile = -1;
    m_iProfilesCount = 0;
}

CDlgProfiles::~CDlgProfiles()
{
}


void CDlgProfiles::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);

    DDX_Control(pDX, IDC_PROFILESLIST, m_ctlProfilesList);
    DDX_Control(pDX, IDC_PROFILECONTENT, m_ctlProfileContent);
    DDX_Control(pDX, IDC_PROFILES_ADD, m_btnAdd);
    DDX_Control(pDX, IDC_PROFILES_REMOVE, m_btnRemove);
    DDX_Control(pDX, IDC_PROFILES_EDITSAVE, m_btnEditSave);

}

BOOL CDlgProfiles::OnInitDialog()
{
    CDialog::OnInitDialog();

    ReloadProfileList();

    m_iSelectedProfile = m_iProfilesCount > 0 ? 0 : -1;
    m_ctlProfilesList.SetCurSel(m_iSelectedProfile);
    m_bEditing = FALSE;

    UpdateControls();

    return TRUE;
}

void CDlgProfiles::OnBtnAddClicked()
{
    CFileDialog dlg(TRUE);
    OPENFILENAME& ofn = dlg.GetOFN();
    TCHAR szFilter[128];
    TCHAR szFile[1024];
    size_t offset;

    _tcscpy_s(szFilter, _countof(szFilter), _T("OVPN config files"));
    offset = _tcslen(szFilter) + 1;
    _stprintf_s(szFilter + offset, _countof(szFilter) - offset, _T("*.%s"), m_pConfig->GetOVPNConfigExt());
    offset += _tcslen(szFilter + offset) + 1;
    szFilter[offset] = _T('\0');

    szFile[0] = _T('\0');

    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = GetSafeHwnd();
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = _countof(szFile);
    ofn.lpstrFilter = szFilter;
    ofn.nFilterIndex = 1;
    ofn.Flags |= OFN_ALLOWMULTISELECT | OFN_FILEMUSTEXIST | OFN_FORCESHOWHIDDEN | OFN_EXPLORER;

    if (dlg.DoModal() != IDOK) return;

    DWORD dwAttr = GetFileAttributes(szFile);
    if (dwAttr == INVALID_FILE_ATTRIBUTES) return;
    
    if (dwAttr & FILE_ATTRIBUTE_DIRECTORY) {
        TCHAR szPath[MAX_PATH];
        _tcscpy_s(szPath, _countof(szPath), szFile);
        offset = _tcslen(szPath);
        szPath[offset++] = _T('\\');
        
        for (LPTSTR szPart = szFile + offset; *szPart; szPart += _tcslen(szPart) + 1) {
            _tcscpy_s(szPath + offset, _countof(szPath) - offset, szPart);
            m_pConfig->AddProfile(szPath);
        }
    }
    else {
        m_pConfig->AddProfile(szFile);
    }

    ReloadProfileList();

    m_iSelectedProfile = m_iProfilesCount > 0 ? 0 : -1;
    m_ctlProfilesList.SetCurSel(m_iSelectedProfile);

    UpdateControls();
}

void CDlgProfiles::OnBtnRemoveClicked()
{
    m_pConfig->RemoveProfile(m_iSelectedProfile);
    ReloadProfileList();

    while (m_iSelectedProfile >= m_iProfilesCount) m_iSelectedProfile--;
    m_ctlProfilesList.SetCurSel(m_iSelectedProfile);

    UpdateControls();
}

void CDlgProfiles::OnProfileListSelChange()
{
    m_iSelectedProfile = m_ctlProfilesList.GetCurSel();
    m_bEditing = FALSE;

    UpdateControls();
}

void CDlgProfiles::OnBtnEditSaveClicked()
{
    if (m_bEditing) {
        VPN_PROFILE_T profile;
        if (!m_pConfig->GetProfile(m_iSelectedProfile, profile)) return;

        if (m_ctlProfileContent.GetWindowTextLength() >= DEF_MAX_PROFILE_LENGTH) {
            CString csText;
            csText.LoadString(IDS_ERR_PROFILE_TOO_BIG);
            DisplayErrorBox(csText);
            return;
        }

        HANDLE hFile = CreateFile(profile.csConfPath, FILE_GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            CString csText;
            csText.Format(IDS_ERR_FILE_OPEN, profile.csConfPath, GetLastError());
            DisplayErrorBox(csText);
            return;
        }
        else {
            TCHAR szContent[DEF_MAX_PROFILE_LENGTH];
            m_ctlProfileContent.GetWindowText(szContent, _countof(szContent));

            DWORD dwBytes;
            CT2AEX<DEF_MAX_PROFILE_LENGTH> content(szContent, CP_UTF8);
            WriteFile(hFile, content, strlen(content), &dwBytes, NULL);
            CLOSEFILE(hFile);
        }
    }

    m_bEditing = !m_bEditing;

    UpdateControls();
}

void CDlgProfiles::ReloadProfileList()
{
    VPN_PROFILE_T profile;
    UINT index = 0;

    m_ctlProfilesList.ResetContent();

    m_iProfilesCount = m_pConfig->ReloadProfiles();
    while (m_pConfig->GetProfile(index++, profile)) {
        m_ctlProfilesList.AddString(profile.csName);
    }
}

void CDlgProfiles::UpdateControls()
{
    if (m_iSelectedProfile >= 0) {
        VPN_PROFILE_T profile;
        if (!m_pConfig->GetProfile(m_iSelectedProfile, profile)) return;

        HANDLE hFile = CreateFile(profile.csConfPath, FILE_GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile == INVALID_HANDLE_VALUE) {
            CString csText;
            csText.Format(IDS_ERR_FILE_OPEN, profile.csConfPath, GetLastError());
            DisplayErrorBox(csText);
            return;
        }
        else {
            BYTE content[DEF_MAX_PROFILE_LENGTH];
            DWORD dwBytes = 0;
            ReadFile(hFile, content, sizeof(content), &dwBytes, NULL);
            if (dwBytes == sizeof(content)) {
                CString csText;
                csText.LoadString(IDS_ERR_PROFILE_TOO_BIG);
                DisplayErrorBox(csText);
                return;
            }
            else {
                ZeroMemory(content + dwBytes, sizeof(content) - dwBytes);
                CA2TEX<DEF_MAX_PROFILE_LENGTH> szContent((LPCSTR)content, CP_UTF8);
                m_ctlProfileContent.SetWindowText(szContent);
            }
            CLOSEFILE(hFile);
        }
    }

    CString csLabel;
    csLabel.LoadString(m_bEditing ? IDS_STR_SAVE : IDS_STR_EDIT);
    m_btnEditSave.SetWindowText(csLabel);

    m_btnAdd.EnableWindow(!m_bEditing);
    m_btnRemove.EnableWindow(!m_bEditing && m_iProfilesCount > 0);
    m_btnEditSave.EnableWindow(m_iSelectedProfile >= 0);
    m_ctlProfileContent.EnableWindow(m_bEditing);
}

void CDlgProfiles::DisplayErrorBox(LPCTSTR szText)
{
    CString csCaption;
    csCaption.LoadString(IDS_STR_ERROR);
    MessageBox(szText, csCaption, MB_ICONERROR | MB_OK);
}
