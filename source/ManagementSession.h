
#pragma once


class CManagementSession
{
public:
    // session arguments
    struct ARGS_T
    {
        UINT uiPort;
        UINT uiBytecountInterval;
        CStringA csPasswordA;
        HANDLE hDataReadyEvent;
        HANDLE hSessionReadyEvent;

        BOOL Validate() { return uiPort > 0 && uiBytecountInterval > 0 && !csPasswordA.IsEmpty() && hDataReadyEvent && hSessionReadyEvent; };
        void Reset() { uiPort = 0; uiBytecountInterval = 0 ; csPasswordA.Empty(); hDataReadyEvent = NULL; hSessionReadyEvent = NULL; };
    };

    struct AUTHINFO_T
    {
        CStringA csUserA;
        CStringA csPasswordA;
    };

    // message IDs back to controller (used as flags)
    enum EVENT_T
    {
        EVENT_STATUS    = 1,
        EVENT_BYTECOUNT = (1 << 1),
        EVENT_PASSWORD  = (1 << 2)
    };

    // bytecount data
    struct BYTECOUNTS_T
    {
        UINT uiIndex;       // index for next write
        struct COUNT_T
        {
            ULONGLONG ullBytesIn;
            ULONGLONG ullBytesOut;
            FILETIME ftTimestamp;
        } data[5];          // 5 last bytecounts
    };

    // password request type
    enum PASSREQUEST_T
    {
        PASSREQ_NONE,
        PASSREQ_AUTH,
        PASSREQ_PRIVATEKEY
    };

public:
    CManagementSession();
    ~CManagementSession();

    BOOL Start(CManagementSession::ARGS_T &args);
    void Stop(BOOL bForce = FALSE);
    void SetAuthInfo(AUTHINFO_T &auth);
    void SetAuthCancel();

    void SetCommand(VPN_COMMAND command);

    DWORD GetEvents();     // combinations of EVENT_T values

    CString GetExternalIP();
    void GetByteCounts(BYTECOUNTS_T &bytecounts, FILETIME & ftConnectTime);
    PASSREQUEST_T GetPasswordRequestType();
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
        VPN_STATUS status;
        CString csExternalIP;
        PASSREQUEST_T passwordType;

        _DATA_T() { InitializeCriticalSection(&crs); Reset(); };
        ~_DATA_T() { DeleteCriticalSection(&crs); };
        void Reset() {
            dwEvents = 0;
            ZeroMemory(&bytecounts, sizeof(bytecounts));
            ZeroMemory(&ftConnectTime, sizeof(ftConnectTime));
            status = VPN_ST_DISCONNECTED;
            csExternalIP.Empty();
            passwordType = PASSREQ_NONE;
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

    CRITICAL_SECTION m_crsCommand;
    VPN_COMMAND m_command;
    BOOL m_bWaitingForAuth;
    AUTHINFO_T m_auth;

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

    void InsertDataStatus(VPN_STATUS status, LPSTR szArg = NULL);
    void InsertDataBytecount(ULONGLONG ullBytesIn, ULONGLONG ullBytesOut);
    void InsertDataPassword(PASSREQUEST_T authType);
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
