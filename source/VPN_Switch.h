
#pragma once

#include "MainWnd.h"


class CVPN_SwitchApp : public CWinApp
{
public:
	CVPN_SwitchApp();

// Overrides
public:
	virtual BOOL InitInstance();
    virtual int ExitInstance();

protected:
    HANDLE m_hUniqueMutex;
    CShellManager *m_pShellManager;
    CMainWnd *m_pWnd;

};
