#include "stdafx.h"
#include "DlgAuth.h"

CDlgAuth::CDlgAuth(LPCTSTR szProfileName, CWnd * pParentWnd)
    : CDialog(IDD, pParentWnd)
{
    m_csProfileName = szProfileName;
}

CDlgAuth::~CDlgAuth()
{
}

void CDlgAuth::DoDataExchange(CDataExchange * pDX)
{
    DDX_Text(pDX, IDC_AUTH_USER, m_csUser);
    DDX_Text(pDX, IDC_AUTH_PASS, m_csPass);
}

BOOL CDlgAuth::OnInitDialog()
{
    CDialog::OnInitDialog();

    CRect dlg_rect, screen_rect;
    GetWindowRect(&dlg_rect);
    SystemParametersInfo(SPI_GETWORKAREA, 0, &screen_rect, 0);

    dlg_rect.MoveToXY(
        screen_rect.right - dlg_rect.Width(),
        screen_rect.bottom - dlg_rect.Height());
    MoveWindow(&dlg_rect, TRUE);

    SetWindowText(m_csProfileName);
    GetDlgItem(IDC_AUTH_USER)->SetWindowText(_T("user"));
    GetDlgItem(IDC_AUTH_PASS)->SetWindowText(_T("pass"));

    return TRUE;
}
