
// http://openvpn.net/index.php/open-source/documentation/miscellaneous/79-management-interface.html

#include "stdafx.h"
#include "ManagementSession.h"


#define COMM_BUFFER_SIZE    4096

#define CMD_BYTECOUNT       "bytecount %u"
#define CMD_HOLD_OFF        "hold off"
#define CMD_HOLD_RELEASE    "hold release"
#define CMD_LOG_ON_ALL      "log on all"
#define CMD_SIGNAL_SIGTERM  "signal SIGTERM"
#define CMD_STATE_ON        "state on"
#define CMD_AUTH_USER       "username \"Auth\" %s"
#define CMD_AUTH_PASSWORD   "password \"Auth\" %s"
#define CMD_EXIT            "exit"

#define MSG_INIT_AUTH       "ENTER PASSWORD"
#define MSG_SUCCESS         "SUCCESS"
#define MSG_ERROR           "ERROR"
#define MSG_INFO            ">INFO"
#define MSG_HOLD            ">HOLD"
#define MSG_STATE           ">STATE"
#define MSG_BYTECOUNT       ">BYTECOUNT"
#define MSG_PASSWORD        ">PASSWORD"
#define MSG_LOG             ">LOG"
#define MSG_LOGEND          "END"

#define ARG_AUTH_OK         "password is correct"
#define ARG_LOG_OK          "real-time log notification set to ON"
#define ARG_HOLD_OK         "hold release succeeded"  

#define ARG_PASSREQ_AUTH    "Need \'Auth\' username/password"
#define ARG_PASSREQ_PKEY    "Need \'Private Key\' password"

#define STATE_CONNECTED     "CONNECTED"
#define STATE_RECONNECTING  "RECONNECTING"
#define STATE_EXITING       "EXITING"

CManagementSession::CManagementSession()
{
    m_hThread           = NULL;
    m_args.Reset();

    m_pSendBuffer       = NULL;
    m_hSendEvent        = NULL;
    m_hCommandEvent     = NULL;
    m_hStopEvent        = NULL;

    m_phase             = PHASE_NONE;

    m_command           = VPN_CMD_NONE;
    m_bWaitingForAuth   = FALSE;
    InitializeCriticalSection(&m_crsCommand); 
}

CManagementSession::~CManagementSession()
{
    DeleteCriticalSection(&m_crsCommand);
}

BOOL CManagementSession::Init(CManagementSession::ARGS_T &args)
{
    BOOL bRet = FALSE;

    do {
        if (!args.Validate()) break;

        m_args = args;

        m_hSendEvent        = CreateEvent(NULL, FALSE, FALSE, NULL);
        m_hCommandEvent     = CreateEvent(NULL, FALSE, FALSE, NULL);
        m_hStopEvent        = CreateEvent(NULL, FALSE, FALSE, NULL);
        if (!m_hSendEvent || !m_hCommandEvent || !m_hStopEvent) break;

        m_pSendBuffer = new char[COMM_BUFFER_SIZE];
        if (!m_pSendBuffer) break;

        m_data.Reset();

        bRet = TRUE;
    } while (0);

    return bRet;
}

void CManagementSession::Shutdown()
{
    DESTRUCT(m_pSendBuffer);
    CLOSEHANDLE(m_hSendEvent);
    CLOSEHANDLE(m_hCommandEvent);
    CLOSEHANDLE(m_hStopEvent);
    m_bWaitingForAuth = FALSE;

    m_args.Reset();
}

BOOL CManagementSession::Start(CManagementSession::ARGS_T &args)
{
    BOOL bRet = Init(args);

    if (bRet) {
        m_hThread = CreateThread(NULL, 0, ThreadProc, this, 0, NULL);
        bRet = m_hThread != NULL;
    }
    if (!bRet) {
        Shutdown();
    }

    return bRet;
}

void CManagementSession::Stop(BOOL bForce)
{
    if (m_hThread) {
        if (bForce) {
            SetEvent(m_hStopEvent);
            WaitForSingleObject(m_hThread, DEF_OPENVPN_TIMEOUT * 1000);
        }
        else {
            SetCommand(VPN_CMD_DISCONNECT);
            if (WaitForSingleObject(m_hThread, DEF_OPENVPN_TIMEOUT * 1000) == WAIT_TIMEOUT) {
                SetEvent(m_hStopEvent);
                WaitForSingleObject(m_hThread, DEF_OPENVPN_TIMEOUT * 1000);
            }
        }
        CLOSEHANDLE(m_hThread);
    }
    Shutdown();
}

void CManagementSession::SetAuthInfo(AUTHINFO_T & auth)
{
    EnterCriticalSection(&m_crsCommand);
    m_command = VPN_CMD_AUTHINFO;
    m_auth = auth;
    LeaveCriticalSection(&m_crsCommand);

    SetEvent(m_hCommandEvent);
}

void CManagementSession::SetAuthCancel()
{
    EnterCriticalSection(&m_crsCommand);
    m_command = VPN_CMD_AUTHCANCEL;
    LeaveCriticalSection(&m_crsCommand);

    SetEvent(m_hCommandEvent);
}

void CManagementSession::SetCommand(VPN_COMMAND command)
{
    EnterCriticalSection(&m_crsCommand);
    m_command = command;
    LeaveCriticalSection(&m_crsCommand);

    SetEvent(m_hCommandEvent);
}

VPN_COMMAND CManagementSession::GetCommand()
{
    VPN_COMMAND command;

    EnterCriticalSection(&m_crsCommand);
    command = m_command;
    LeaveCriticalSection(&m_crsCommand);

    return command;
}

CString CManagementSession::GetLocalIP()
{
    CString ret;
    m_data.Lock();
    ret = m_data.csLocalIP;
    m_data.Unlock();
    return ret;
}

CString CManagementSession::GetExternalIP()
{
    CString ret;
    m_data.Lock();
    ret = m_data.csExternalIP;
    m_data.Unlock();
    return ret;
}

void CManagementSession::GetByteCounts(BYTECOUNTS_T & bytecounts, FILETIME & ftConnectTime)
{
    m_data.Lock();
    CopyMemory(&bytecounts, &m_data.bytecounts, sizeof(BYTECOUNTS_T));
    CopyMemory(&ftConnectTime, &m_data.ftConnectTime, sizeof(FILETIME));
    m_data.Unlock();
}

CManagementSession::PASSREQUEST_T CManagementSession::GetPasswordRequestType()
{
    PASSREQUEST_T type;
    m_data.Lock();
    type = m_data.passwordType;
    m_data.Unlock();
    return type;
}

VPN_STATUS CManagementSession::GetStatus()
{
    VPN_STATUS status;

    m_data.Lock();
    status = m_data.status;
    m_data.Unlock();

    return status;
}

DWORD CManagementSession::Main()
{
    int iResult             = 0;
    DWORD dwFlags           = 0;
    DWORD dwBytesSent       = 0;
    DWORD dwBytesReceived   = 0;
    BOOL bStop              = FALSE;
    struct sockaddr_in saOpenVPN        = { 0 };
    WSAOVERLAPPED wsaRecvOverlapped     = { 0 };
    WSABUF wsaSendBuffer                = { 0 };
    WSABUF wsaRecvBuffer                = { 0 };
    char pRecvBuffer[COMM_BUFFER_SIZE];
    HANDLE hWaitObjects[4];
    SOCKET sock = INVALID_SOCKET;


    do {
        InitMessageHandlers();

        sock = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED | WSA_FLAG_NO_HANDLE_INHERIT);
        if (sock == INVALID_SOCKET) {
            InsertDataStatus(VPN_ST_ERROR, "Failed to create socket.");
            break;
        }

        saOpenVPN.sin_family = AF_INET;
        saOpenVPN.sin_port = htons((USHORT)m_args.uiPort);
        InetPton(AF_INET, _T("127.0.0.1"), &saOpenVPN.sin_addr);

        if (WSAConnect(sock, (sockaddr *)&saOpenVPN, sizeof(sockaddr_in), NULL, NULL, NULL, NULL) != 0) {
            InsertDataStatus(VPN_ST_ERROR, "Failed to connect to OpenVPN socket.");
            break;
        }

        m_phase = PHASE_AUTH;

        m_pSendBuffer[0] = '\0';
        wsaSendBuffer.buf = m_pSendBuffer;

        ZeroMemory(pRecvBuffer, sizeof(pRecvBuffer));
        wsaRecvBuffer.buf = pRecvBuffer;
        wsaRecvBuffer.len = sizeof(pRecvBuffer)-1;
        wsaRecvOverlapped.hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

        hWaitObjects[0] = m_hSendEvent;
        hWaitObjects[1] = wsaRecvOverlapped.hEvent;
        hWaitObjects[2] = m_hCommandEvent;
        hWaitObjects[3] = m_hStopEvent;

        iResult = WSARecv(sock, &wsaRecvBuffer, 1, NULL, &dwFlags, &wsaRecvOverlapped, NULL);

        do {
            switch (WaitForMultipleObjects(4, hWaitObjects, FALSE, INFINITE))
            {
            case WAIT_OBJECT_0: // m_hSendEvent
                wsaSendBuffer.len = strlen(m_pSendBuffer);
                iResult = WSASend(sock, &wsaSendBuffer, 1, &dwBytesSent, 0, NULL, NULL);
                m_pSendBuffer[0] = '\0';
                break;
            case WAIT_OBJECT_0 + 1: // wsaRecvOverlapped.hEvent
                HandleReceive(pRecvBuffer, sizeof(pRecvBuffer));
                ZeroMemory(pRecvBuffer, sizeof(pRecvBuffer));
                iResult = WSARecv(sock, &wsaRecvBuffer, 1, &dwBytesReceived, &dwFlags, &wsaRecvOverlapped, NULL);
                if (iResult == 0 && dwBytesReceived == 0) bStop = TRUE;
                break;
            case WAIT_OBJECT_0 + 2: // m_hCommandEvent
                HandleCommand();
                break;
            case WAIT_OBJECT_0 + 3: // m_hStopEvent
                bStop = TRUE;
                break;
            default:
                InsertDataStatus(VPN_ST_ERROR, "Unknown fatal error!");
                bStop = TRUE;
                break;
            }
        } while (!bStop);

    } while (0);


    if (sock != INVALID_SOCKET) closesocket(sock);
    
    if (m_phase == PHASE_SHUTDOWN) InsertDataStatus(VPN_ST_DISCONNECTED);

    CLOSEHANDLE(wsaRecvOverlapped.hEvent);

    return 0;
}

void CManagementSession::HandleCommand()
{
    switch (GetCommand())
    {
    case VPN_CMD_AUTHCANCEL:
    {
        if (!m_bWaitingForAuth) break;
        m_bWaitingForAuth = FALSE;
    }
    case VPN_CMD_DISCONNECT:
    {
        m_phase = PHASE_SHUTDOWN;
        QueueSendMessage(CMD_SIGNAL_SIGTERM);
        QueueSendMessage(CMD_EXIT);
        SetEvent(m_hSendEvent);
        break;
    }
    case VPN_CMD_AUTHINFO:
    {
        if (m_bWaitingForAuth) {
            CStringA user, password;
            EnterCriticalSection(&m_crsCommand);
            user.Format(CMD_AUTH_USER, m_auth.csUserA.GetBuffer());
            password.Format(CMD_AUTH_PASSWORD, m_auth.csPasswordA.GetBuffer());
            LeaveCriticalSection(&m_crsCommand);

            QueueSendMessage(user);
            QueueSendMessage(password);
            SetEvent(m_hSendEvent);
            m_bWaitingForAuth = FALSE;
        }
    }
    default:
        break;
    }

}

void CManagementSession::HandleReceive(LPSTR buffer, DWORD dwSize)
{
    FMessageHandler handler = NULL;
    LPSTR szMessage         = NULL;
    LPSTR szArg             = NULL;
    POSITION index          = NULL;
    MESSAGE_HANDLE_T messageHandle;

    while (buffer && *buffer) {
        szMessage = buffer;
        
        buffer = strstr(szMessage, "\r\n");
        if (buffer) {
            *(buffer++) = '\0';
            *(buffer++) = '\0';
        }

        if (m_phase == PHASE_LOG) {
            MessageHandlerLog(szMessage);
            continue;
        }

        szArg = strchr(szMessage, ':');
        if (szArg) {
            *(szArg++) = '\0';
            while (*szArg == ' ') {
                if (!*(++szArg)) break;
            }
        }

        index = m_messageHandlers.GetHeadPosition();
        do {
            messageHandle = m_messageHandlers.GetNext(index);
            if (messageHandle.csMessageA.CompareNoCase(szMessage) == 0) {
                (this->*(messageHandle.handler))(szArg);
                break;
            }
        } while (index);
    }

}

void CManagementSession::QueueSendMessage(LPCSTR szMessage)
{
    strcat_s(m_pSendBuffer, COMM_BUFFER_SIZE, szMessage);
    strcat_s(m_pSendBuffer, COMM_BUFFER_SIZE, "\r\n");
}

void CManagementSession::InitMessageHandlers()
{
    m_messageHandlers.RemoveAll();

    m_messageHandlers.AddTail({ MSG_LOG,          &CManagementSession::MessageHandlerLog });
    m_messageHandlers.AddTail({ MSG_BYTECOUNT,    &CManagementSession::MessageHandlerBytecount });
    m_messageHandlers.AddTail({ MSG_STATE,        &CManagementSession::MessageHandlerState });
    m_messageHandlers.AddTail({ MSG_SUCCESS,      &CManagementSession::MessageHandlerSuccess });
    m_messageHandlers.AddTail({ MSG_ERROR,        &CManagementSession::MessageHandlerError });
    m_messageHandlers.AddTail({ MSG_HOLD,         &CManagementSession::MessageHandlerHold });
    m_messageHandlers.AddTail({ MSG_INIT_AUTH,    &CManagementSession::MessageHandlerAuth });
    m_messageHandlers.AddTail({ MSG_PASSWORD,     &CManagementSession::MessageHandlerPassword });
}

void CManagementSession::MessageHandlerAuth(LPSTR szLine)
{
    QueueSendMessage(m_args.csPasswordA);
    
    SetEvent(m_hSendEvent);
}

void CManagementSession::MessageHandlerHold(LPSTR szLine)
{
    char szBytecount[64];
    sprintf_s(szBytecount, _countof(szBytecount), CMD_BYTECOUNT, m_args.uiBytecountInterval);

    QueueSendMessage(CMD_LOG_ON_ALL);
    QueueSendMessage(CMD_STATE_ON);
    QueueSendMessage(szBytecount);
    QueueSendMessage(CMD_HOLD_OFF);
    QueueSendMessage(CMD_HOLD_RELEASE);
    
    SetEvent(m_hSendEvent);
    SetEvent(m_args.hSessionReadyEvent);    // here we know we're connected to OpenVPN
}



void CManagementSession::MessageHandlerLog(LPSTR szLine)
{
    if (m_phase == PHASE_LOG) {
        if (strcmp(szLine, MSG_LOGEND) == 0) {
            m_phase = PHASE_INIT;
        }
        return;
    }

    LPSTR szToken       = NULL;
    LPSTR szContext     = NULL;

    szToken = strtok_s(szLine, ",", &szContext);
    szToken = strtok_s(szContext, ",", &szContext);

    switch (*szToken) {
    case 'F':   // fatal error
    case 'N':   // non-fatal error
    {
        szToken = strtok_s(szContext, ",", &szContext);
        InsertDataStatus(VPN_ST_ERROR, szToken);
        break;
    }
    case 'W':   // warning
    case 'I':   // informational
    case 'D':   // debug
    default:
        break;
    }
}

void CManagementSession::MessageHandlerState(LPSTR szLine)
{
    LPSTR szToken       = NULL;
    LPSTR szContext     = NULL;

    // >STATE:datetime,state,description,local_IP,remote_IP

    szToken = strtok_s(szLine, ",", &szContext);        // datetime
    szToken = strtok_s(szContext, ",", &szContext);     // state

    if (strcmp(szToken, STATE_CONNECTED) == 0) {
        szToken = strtok_s(szContext, ",", &szContext); // description
        if (strcmp(szToken, MSG_SUCCESS) == 0) {
            LPSTR szLocalIP = strtok_s(szContext, ",", &szContext);
            LPSTR szExternalIP = strtok_s(szContext, ",", &szContext);
            InsertDataStatus(VPN_ST_CONNECTED, szLocalIP, szExternalIP);
        }
    }
    else if (strcmp(szToken, STATE_RECONNECTING) == 0) {
        InsertDataStatus(VPN_ST_CONNECTING);
    }
    else if (strcmp(szToken, STATE_EXITING) == 0) {
        InsertDataStatus(VPN_ST_DISCONNECTED);
        SetEvent(m_hStopEvent);
    }
}

void CManagementSession::MessageHandlerBytecount(LPSTR szLine)
{
    ULONGLONG ullBytesIn    = 0;
    ULONGLONG ullBytesOut   = 0;
    LPSTR szToken   = NULL;
    LPSTR szContext = NULL;

    if (m_phase != PHASE_MAIN) return;

    szToken = strtok_s(szLine, ",", &szContext);
    ullBytesIn = atoll(szToken);

    szToken = strtok_s(szContext, ",", &szContext);
    ullBytesOut = atoll(szToken);

    InsertDataBytecount(ullBytesIn, ullBytesOut);
}

void CManagementSession::MessageHandlerPassword(LPSTR szLine)
{
    PASSREQUEST_T type = PASSREQ_NONE;
    if (strcmp(szLine, ARG_PASSREQ_AUTH) == 0)
        type = PASSREQ_AUTH;
    else if (strcmp(szLine, ARG_PASSREQ_PKEY) == 0)
        type = PASSREQ_PRIVATEKEY;

    if (type != PASSREQ_NONE) {
        m_bWaitingForAuth = TRUE;
        InsertDataPassword(type);
    }
}

void CManagementSession::MessageHandlerSuccess(LPSTR szLine)
{
    switch (m_phase) {
    case PHASE_AUTH:
        if (strcmp(szLine, ARG_AUTH_OK) == 0) {
            m_phase = PHASE_INIT;
        }
        break;
    case PHASE_INIT:
        if (strcmp(szLine, ARG_LOG_OK) == 0) {
            m_phase = PHASE_LOG;
        }
        else if (strcmp(szLine, ARG_HOLD_OK) == 0) {
            m_phase = PHASE_MAIN;
        }
        break;
    case PHASE_MAIN:
        break;
    default:
        break;
    }
}

void CManagementSession::MessageHandlerError(LPSTR szLine)
{
    InsertDataStatus(VPN_ST_ERROR, szLine);
}

void CManagementSession::InsertDataStatus(VPN_STATUS status, LPSTR szArg1, LPSTR szArg2)
{
    m_data.Lock();
    if (status == VPN_ST_CONNECTED) {
        if (szArg1) m_data.csLocalIP = szArg1;
        if (szArg2) m_data.csExternalIP = szArg2;
        GetSystemTimeAsFileTime(&m_data.ftConnectTime);
    }
    m_data.status = status;
    m_data.dwEvents |= EVENT_STATUS;
    m_data.Unlock();
   
    SetEvent(m_args.hDataReadyEvent);
}

void CManagementSession::InsertDataBytecount(ULONGLONG ullBytesIn, ULONGLONG ullBytesOut)
{
    m_data.Lock();
    BYTECOUNTS_T::COUNT_T & cnt = m_data.bytecounts.data[m_data.bytecounts.uiIndex];
    cnt.ullBytesIn = ullBytesIn;
    cnt.ullBytesOut = ullBytesOut;
    GetSystemTimeAsFileTime(&cnt.ftTimestamp);
    (++m_data.bytecounts.uiIndex) %= _countof(m_data.bytecounts.data);
    m_data.dwEvents |= EVENT_BYTECOUNT;
    m_data.Unlock();

    SetEvent(m_args.hDataReadyEvent);
}

void CManagementSession::InsertDataPassword(PASSREQUEST_T authType)
{
    m_data.Lock();
    m_data.passwordType = authType;
    m_data.dwEvents |= EVENT_PASSWORD;
    m_data.Unlock();
    SetEvent(m_args.hDataReadyEvent);
}

void CManagementSession::InsertDataLog(LPSTR szLine)
{

}

DWORD CManagementSession::GetEvents()
{
    DWORD dwEvents;

    m_data.Lock();
    dwEvents = m_data.dwEvents;
    m_data.dwEvents = 0;
    m_data.Unlock();

    return dwEvents;
}


DWORD CManagementSession::ThreadProc(LPVOID pContext)
{
    return ((CManagementSession *)pContext)->Main();
}
