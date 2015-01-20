


#pragma once

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN
#endif

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS
#define _AFX_ALL_WARNINGS


#include "targetver.h"          // target OS version

#include <afxwin.h>             // MFC core and standard components
#include <afxsock.h>            // MFC Winsock extension
#include <afxcmn.h>             // MFC support for Windows Common Controls
#include <afxcontrolbars.h>     // MFC support for ribbons and control bars

#include <atlcoll.h>            // ATL collection classes

#include "defs.h"               // global header

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#ifdef _UNICODE
#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
#endif
