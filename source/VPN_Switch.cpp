
// VPN_Switch.cpp : Defines the class behaviors for the application.
//

#include "stdafx.h"
#include "Utils.h"
#include "MainWnd.h"

#include "VPN_Switch.h"

CVPN_SwitchApp theApp;      // global app object


CVPN_SwitchApp::CVPN_SwitchApp()
{
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

    // Create the shell manager, in case the dialog contains
    // any shell tree view or shell list view controls.
    m_pShellManager = new CShellManager;
    if (!m_pShellManager) return FALSE;

    // init winsock
    if (!AfxSocketInit()) return FALSE;

	// Activate "Windows Native" visual manager for enabling themes in MFC controls
	CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));

    // Create window
    CMainWnd *pMainWindow = new CMainWnd;
    if (!pMainWindow || !pMainWindow->Create()) return FALSE;
    m_pMainWnd = pMainWindow;
    
    // frame window attempt
    //pMainWindow->LoadFrame(IDR_TRAY_MENU);
    //m_nCmdShow = SW_HIDE;
    //pMainWindow->InitialUpdateFrame(NULL, TRUE);

	return TRUE;
}

int CVPN_SwitchApp::ExitInstance()
{
    CLOSEHANDLE(m_hUniqueMutex);
    DESTRUCT(m_pShellManager);

    return CWinApp::ExitInstance();
}
