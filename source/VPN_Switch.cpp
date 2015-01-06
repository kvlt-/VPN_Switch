
// VPN_Switch.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "Utils.h"
#include "MainWnd.h"

#include "VPN_Switch.h"

CVPN_SwitchApp theApp;      // global app object


CVPN_SwitchApp::CVPN_SwitchApp()
{
    // support Restart Manager
    m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_RESTART;
    m_hUniqueMutex = NULL;
}

BOOL CVPN_SwitchApp::InitInstance()
{
    // init common controls
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

    // init CWinApp base
	CWinApp::InitInstance();

    // ensure one application instance
    m_hUniqueMutex = CUtils::GetUniqueGlobalMutex(DEF_UNIQUE_MUTEX_NAME);
    if (!m_hUniqueMutex) return FALSE;

    // init winsock
	if (!AfxSocketInit()) return FALSE;

	// Create the shell manager, in case the dialog contains
	// any shell tree view or shell list view controls.
	CShellManager *pShellManager = new CShellManager;

	// Activate "Windows Native" visual manager for enabling themes in MFC controls
	CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));

    // Run main loop
    CMainWnd MainWindow;
    MainWindow.Run();
    
	// Delete the shell manager created above.
    if (pShellManager != NULL) delete pShellManager;

	return FALSE;
}

int CVPN_SwitchApp::ExitInstance()
{
    CWinApp::ExitInstance();

    CLOSEHANDLE(m_hUniqueMutex);

    return 0;
}
