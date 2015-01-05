#pragma once

#include "Configuration.h"

class CDlgProfiles : public CDialog
{
    enum { IDD = IDD_EDITPROFILES };

public:

    CDlgProfiles(CConfiguration *pConfig, CWnd *pParentWnd = NULL);
    virtual ~CDlgProfiles();

    virtual void DoDataExchange(CDataExchange* pDX);

protected:
    CConfiguration *m_pConfig;

    CListBox    m_ctlProfilesList;
    CEdit       m_ctlProfileContent;
    CButton     m_btnAdd;
    CButton     m_btnRemove;
    CButton     m_btnEditSave;

    BOOL        m_bEditing;
    UINT        m_iSelectedProfile;
    UINT        m_iProfilesCount;

protected:
    virtual BOOL OnInitDialog();

    DECLARE_MESSAGE_MAP()

    afx_msg void OnBtnAddClicked();
    afx_msg void OnBtnRemoveClicked();
    afx_msg void OnBtnEditSaveClicked();
    afx_msg void OnProfileListSelChange();

protected:
    void ReloadProfileList();
    void UpdateControls();

    void DisplayErrorBox(LPCTSTR szText);
};