
#pragma once


class CManagementSession
{
public:
    // session arguments
    typedef struct _ARGS_T
    {
        UINT uiPort;
        UINT uiBytecountInterval;
        CStringA csPasswordA;
        HANDLE hDataReadyEvent;
        HANDLE hSessionReadyEvent;

        BOOL Validate() { return uiPort >= DEF_PORT_MIN && uiPort <= DEF_PORT_MAX && uiBytecountInterval > 0 && !csPasswordA.IsEmpty() && hDataReadyEvent && hSessionReadyEvent; };
        void Reset() { uiPort = 0; uiBytecountInterval = 0 ; csPasswordA.Empty(); hDataReadyEvent = NULL; hSessionReadyEvent = NULL; };
    } ARGS_T;

    // message IDs back to controller (used as flags)
    typedef enum _EVENT_T
    {
        EVENT_STATUS = 1,
        EVENT_BYTECOUNT = 2
    } EVENT_T;

    // bytecount data
    typedef struct _BYTECOUNT_T
    {
        ULONGLONG ullBytesIn;
        ULONGLONG ullBytesOut;
        FILETIME ftTimestamp;
    } BYTECOUNT_T;

    typedef struct _BYTECOUNTS_T
    {
        UINT uiIndex;           // index for next write
        BYTECOUNT_T data[5];    // 5 last bytecounts
    } BYTECOUNTS_T;

public:
    CManagementSession();
    ~CManagementSession();

    BOOL Start(CManagementSession::ARGS_T &args);
    void Stop(BOOL bForce = FALSE);

    void SetCommand(VPN_COMMAND command);

    DWORD GetEvents();     // combinations of EVENT_T values

    void GetExternalIP(CString &csExternalIP);
    void GetByteCounts(BYTECOUNTS_T *pBytecounts, FILETIME *pftConnectTime);
    VPN_STATUS GetStatus();

protected:
    typedef enum
    {
        PHASE_NONE,
        PHASE_AUTH,
        PHASE_INIT,
        PHASE_LOG,
        PHASE_MAIN,
        PHASE_SHUTDOWN
    } PHASE_T;

    typedef struct _DATA_T
    {
        CRITICAL_SECTION crs;
        DWORD dwEvents;
        CManagementSession::BYTECOUNTS_T bytecounts;
        FILETIME ftConnectTime;
        CString csExternalIP;
        VPN_STATUS status;

        _DATA_T() { InitializeCriticalSection(&crs); Reset(); };
        ~_DATA_T() { DeleteCriticalSection(&crs); };
        void Reset() {
            dwEvents = 0;
            ZeroMemory(&bytecounts, sizeof(bytecounts));
            ZeroMemory(&ftConnectTime, sizeof(ftConnectTime));
            csExternalIP.Empty();
            status = VPN_ST_DISCONNECTED;
        }
        void Lock() { EnterCriticalSection(&crs); };
        void Unlock() { LeaveCriticalSection(&crs); };
    } DATA_T;

    // OpenVPN message handler function type
    typedef void(CManagementSession::*FMessageHandler) (LPSTR);
    // OpenVPN message -> handler mapping
    typedef struct _MESSAGE_HANDLE_T
    {
        CStringA csMessageA;
        FMessageHandler handler;
    } MESSAGE_HANDLE_T;
    // message -> handler mapping list
    typedef CAtlList<MESSAGE_HANDLE_T> MESSAGE_HANDLE_LIST_T;

protected:

    HANDLE m_hThread;
    HANDLE m_hCommandEvent;
    HANDLE m_hStopEvent;

    MESSAGE_HANDLE_LIST_T m_messageHandlers;
    LPSTR m_pSendBuffer;
    HANDLE m_hSendEvent;

    VPN_COMMAND m_command;
    CRITICAL_SECTION m_crsCommand;

    CManagementSession::ARGS_T m_args;
    CManagementSession::DATA_T m_data;
    CManagementSession::PHASE_T m_phase;

protected:
    BOOL Init(CManagementSession::ARGS_T &args);
    void Shutdown();

    DWORD Main();

    void InitMessageHandlers();
    void HandleCommand();
    void HandleReceive(LPSTR buffer, DWORD dwSize);
    void QueueSendMessage(LPCSTR szMessage);

    VPN_COMMAND GetCommand();

    void InsertDataStatus(VPN_STATUS status, LPSTR szExternalIP = NULL);
    void InsertDataBytecount(ULONGLONG, ULONGLONG);
    void InsertDataLog(LPSTR szLine);

    void MessageHandlerAuth(LPSTR);
    void MessageHandlerHold(LPSTR);
    void MessageHandlerLog(LPSTR);
    void MessageHandlerState(LPSTR);
    void MessageHandlerBytecount(LPSTR);
    void MessageHandlerPassword(LPSTR);
    void MessageHandlerSuccess(LPSTR);
    void MessageHandlerError(LPSTR);

private:
    static DWORD WINAPI ThreadProc(LPVOID);

};
