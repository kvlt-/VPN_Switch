
// VPN_Switch.h : main header file for the PROJECT_NAME application
//

#pragma once

#ifndef __AFXWIN_H__
	#error "include 'stdafx.h' before including this file for PCH"
#endif



// CVPN_SwitchApp:
// See VPN_Switch.cpp for the implementation of this class
//

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
};

extern CVPN_SwitchApp theApp;