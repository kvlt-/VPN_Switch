#include "stdafx.h"
#include <TlHelp32.h>

#include "Utils.h"


BOOL CUtils::IsProcessRunning(LPCTSTR szFileName)
{
    BOOL bIsRunning     = FALSE;
    HANDLE hSnapshot    = INVALID_HANDLE_VALUE;
    PROCESSENTRY32 processInfo = {0};

    do {
        processInfo.dwSize = sizeof(processInfo);

        hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);
        if (hSnapshot == INVALID_HANDLE_VALUE) break;

        if (!Process32First(hSnapshot, &processInfo)) break;
        do {
            if (_tcsicmp(szFileName, processInfo.szExeFile) == 0) {
                bIsRunning = TRUE;
                break;
            }
        } while (Process32Next(hSnapshot, &processInfo));
    } while (0);

    CLOSEFILE(hSnapshot);

    return bIsRunning;
}

BOOL CUtils::IsPortAvailable(UINT uiPort)
{
    SOCKET sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, 0);
    if (sock == INVALID_SOCKET) return TRUE;

    sockaddr_in sa;
    sa.sin_family = AF_INET;
    sa.sin_port = htons((USHORT)uiPort);
    InetPton(AF_INET, _T("127.0.0.1"), &sa.sin_addr);

    BOOL bAvailable = TRUE;
    if (bind(sock,(sockaddr *)&sa, sizeof(sa)) == SOCKET_ERROR) {
        int iError = WSAGetLastError();
        bAvailable = (iError != WSAEADDRINUSE && iError != WSAEACCES);
    }
    
    closesocket(sock);

    return bAvailable;
}

HANDLE CUtils::GetUniqueSessionEvent(LPCTSTR szPrefix, CString &csEventName)
{
    TCHAR szFormat[MAX_PATH];
    _tcscpy_s(szFormat, _countof(szFormat), szPrefix);
    _tcscat_s(szFormat, _countof(szFormat), _T("%d"));
    srand((unsigned int)time(NULL));

    HANDLE hEvent = NULL;
    for (int i = 0; !hEvent && i < 10; i++) {
        csEventName.Format(szFormat, rand());
        hEvent = CreateEvent(NULL, TRUE, FALSE, CString(_T("Local\\") + csEventName));
        if (hEvent && GetLastError() == ERROR_ALREADY_EXISTS) CLOSEHANDLE(hEvent);
    } 

    return hEvent;
}

void CUtils::ConvertRelativeToFullPath(CString &csPath, CString &csBaseDir)
{
    if (csPath.GetLength() > 2 && csPath.GetAt(1) == _T(':')) return;

    TCHAR szBuffer[MAX_PATH];
    _stprintf_s(szBuffer, _countof(szBuffer), _T("%s\\%s"), csBaseDir, csPath);

    csPath = szBuffer;
}

void CUtils::ConvertFullToRelativePath(CString &csPath, CString &csBaseDir)
{
    if (_tcsnicmp(csPath, csBaseDir, csBaseDir.GetLength()) != 0) return;

    LPTSTR szBuffer = csPath.GetBuffer() + csBaseDir.GetLength();
    if (*szBuffer == _T('\\')) szBuffer++;

    csPath = szBuffer;
}

void CUtils::FormatTimeElapsed(ULONGLONG ullTime, LPTSTR szDest, DWORD dwDestSize)
{
    SYSTEMTIME time;
    time.wDay = (WORD)(ullTime / (3600 * 24));
    ullTime -= (ULONGLONG)time.wDay * (3600 * 24);
    time.wHour = (WORD)(ullTime / 3600);
    ullTime -= (ULONGLONG)time.wHour * 3600;
    time.wMinute = (WORD)(ullTime / 60);
    ullTime -= (ULONGLONG)time.wMinute * 60;
    time.wSecond = (WORD)(ullTime);

    if (time.wDay) {
        _stprintf_s(szDest, dwDestSize, _T("%hdd %hdh %hdm %hds"), time.wDay, time.wHour, time.wMinute, time.wSecond);
    }
    else if (time.wHour) {
        _stprintf_s(szDest, dwDestSize, _T("%hdh %hdm %hds"), time.wHour, time.wMinute, time.wSecond);
    }
    else if (time.wMinute) {
        _stprintf_s(szDest, dwDestSize, _T("%hdm %hds"), time.wMinute, time.wSecond);
    }
    else {
        _stprintf_s(szDest, dwDestSize, _T("%hds"), time.wSecond);
    }
}

void CUtils::FormatSpeedKbs(DWORD dwSpeed, LPTSTR szDest, DWORD dwDestSize)
{
    if (dwSpeed >= 1024) {
        _stprintf_s(szDest, dwDestSize, _T("%.2g MB/s"), dwSpeed / 1024.0);
    }
    else {
        _stprintf_s(szDest, dwDestSize, _T("%u kB/s"), dwSpeed);
    }
}

void CUtils::FormatSizeKb(ULONGLONG ullSize, LPTSTR szDest, DWORD dwDestSize)
{
    if (ullSize > 1024 * 1024) {
        _stprintf_s(szDest, dwDestSize, _T("%.2g GB"), (DOUBLE)ullSize / (1024 * 1024));
    }
    else if (ullSize >= 1024) {
        _stprintf_s(szDest, dwDestSize, _T("%.2g MB"), ullSize / 1024.0);
    }
    else {
        _stprintf_s(szDest, dwDestSize, _T("%llu kB"), ullSize);
    }
}

ULONGLONG CUtils::FiletimeDiff(LPFILETIME pftTime1, LPFILETIME pftTime2)
{
    PLARGE_INTEGER pliTime1 = (PLARGE_INTEGER)pftTime1;
    PLARGE_INTEGER pliTime2 = (PLARGE_INTEGER)pftTime2;

    return pliTime1->QuadPart > pliTime2->QuadPart ? pliTime1->QuadPart - pliTime2->QuadPart : pliTime2->QuadPart - pliTime1->QuadPart;
}
