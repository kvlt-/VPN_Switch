#pragma once

#include "resource.h"


class CDlgAuth : public CDialog
{
    enum { IDD = IDD_AUTH };

public:

    CDlgAuth(LPCTSTR szProfileName, CWnd *pParentWnd = NULL);
    ~CDlgAuth();

    CString GetAuthUser() { return m_csUser; }
    CString GetAuthPassword() { return m_csPass; }

    virtual void DoDataExchange(CDataExchange* pDX);

protected:
    CString     m_csProfileName;
    CString     m_csUser;
    CString     m_csPass;

protected:
    virtual BOOL OnInitDialog() override;
};