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
    static void ConvertRelativeToFullPath(CString &csPath, CString &csBaseDir);
    static void ConvertFullToRelativePath(CString &csPath, CString &csBaseDir);
    static void FormatTimeElapsed(ULONGLONG ullTime, LPTSTR szDest, DWORD dwDestSize);
    static void FormatSpeedKbs(DWORD dwSpeed, LPTSTR szDest, DWORD dwDestSize);
    static void FormatSizeKb(ULONGLONG ullSize, LPTSTR szDest, DWORD dwDestSize);
    static ULONGLONG FiletimeDiff(LPFILETIME pftTime1, LPFILETIME pftTime2);
};

