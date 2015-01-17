#pragma once
class CTrayNotifier
{
public:
    CTrayNotifier();
    ~CTrayNotifier();

    BOOL Init(HWND hMainWnd, UINT uiTrayMessageID);
    void Shutdown();

    BOOL TrackMouse();

    void Notify(VPN_STATUS status, LPCTSTR szText);
    void ChangeTip(LPCTSTR szTip);

protected:
    HWND m_hMainWnd;
    UINT m_uiTrayMessageID;

    NOTIFYICONDATA m_trayData;

    HANDLE m_hThread;
    HANDLE m_hStopEvent;
    HANDLE m_hTrackStartEvent;

    CRITICAL_SECTION m_crsTrack;
    BOOL m_bTracking;
    POINT m_ptCursor;

protected:
    DWORD ThreadMain();

    BOOL CheckMouseLeave();

private:
    static DWORD WINAPI ThreadProc(LPVOID pContext);

};

