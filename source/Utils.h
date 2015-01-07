#pragma once


class CUtils
{
public:
    CUtils() {};
    ~CUtils() {};

    static BOOL IsProcessRunning(LPCTSTR szFileName);
    static BOOL IsPortAvailable(UINT uiPort);
    static HANDLE GetUniqueSessionEvent(LPCTSTR szPrefix, CString &csEventName);
    static HANDLE GetUniqueGlobalMutex(LPCTSTR szMutexName);
    static CString ConvertRelativeToFullPath(CString &csPath, CString &csBaseDir, BOOL bOverwrite = FALSE);
    static CString ConvertFullToRelativePath(CString &csPath, CString &csBaseDir, BOOL bOverwrite = FALSE);
    static void FormatTimeElapsed(ULONGLONG ullTime, LPTSTR szDest, DWORD dwDestSize);
    static void FormatSpeedKbs(DWORD dwSpeed, LPTSTR szDest, DWORD dwDestSize);
    static void FormatSizeKb(ULONGLONG ullSize, LPTSTR szDest, DWORD dwDestSize);
    static ULONGLONG FiletimeDiff(LPFILETIME pftTime1, LPFILETIME pftTime2);
};

