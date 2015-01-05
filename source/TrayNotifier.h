#pragma once
class CTrayNotifier
{
public:
    CTrayNotifier();
    ~CTrayNotifier();
    BOOL Init(HWND hTrayWnd, UINT uiTrayMessageID);
    void Shutdown();
    BOOL TrackMouse();

protected:
    HWND m_hTrayWnd;
    UINT m_uiTrayMessageID;

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

