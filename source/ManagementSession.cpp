
// http://openvpn.net/index.php/open-source/documentation/miscellaneous/79-management-interface.html

#include "stdafx.h"
#include "ManagementSession.h"


#define COMM_BUFFER_SIZE    2048

#define CMD_BYTECOUNT       "bytecount %u"
#define CMD_HOLD_OFF        "hold off"
#define CMD_HOLD_RELEASE    "hold release"
#define CMD_LOG_ON_ALL      "log on all"
#define CMD_SIGNAL_SIGTERM  "signal SIGTERM"
#define CMD_STATE_ON        "state on"
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

#define STATE_CONNECTED     "CONNECTED"
#define STATE_RECONNECTING  "RECONNECTING"
#define STATE_EXITING       "EXITING"

CManagementSession::CManagementSession()
{
    m_hThread           = NULL;
    m_args.Reset();

    m_hSendEvent        = NULL;
    m_hCommandEvent     = NULL;
    m_hStopEvent        = NULL;

    m_phase             = PHASE_NONE;

    m_command           = VPN_CMD_NONE;
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

    m_args.Reset();
}

BOOL CManagementSession::Start(CManagementSession::ARGS_T &args)
{
    BOOL bRet = Init(args);

    if (bRet) {
        m_hThread = CreateThread(NULL, 0, ThreadProc, this, 0, NULL);
        bRet = m_hThread != NULL;
    }
    else {
        Shutdown();
    }

    if (bRet) SetCommand(VPN_CMD_CONNECT);

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

void CManagementSession::GetExternalIP(CString &csExternalIP)
{
    m_data.Lock();
    csExternalIP = m_data.csExternalIP;
    m_data.Unlock();
}

void CManagementSession::GetByteCounts(BYTECOUNTS_T *pBytecounts, FILETIME *pftConnectTime)
{
    m_data.Lock();
    CopyMemory(pBytecounts, &m_data.bytecounts, sizeof(BYTECOUNTS_T));
    CopyMemory(pftConnectTime, &m_data.ftConnectTime, sizeof(FILETIME));
    m_data.Unlock();
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
    DWORD dwExitCode        = 0;
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
            dwExitCode = 1;
            break;
        }

        WaitForSingleObject(m_hCommandEvent, DEF_OPENVPN_TIMEOUT * 1000);
        if (GetCommand() != VPN_CMD_CONNECT) {
            dwExitCode = 1;
            break;
        }

        saOpenVPN.sin_family = AF_INET;
        saOpenVPN.sin_port = htons((USHORT)m_args.uiPort);
        InetPton(AF_INET, _T("127.0.0.1"), &saOpenVPN.sin_addr);

        if (WSAConnect(sock, (sockaddr *)&saOpenVPN, sizeof(sockaddr_in), NULL, NULL, NULL, NULL) != 0) {
            dwExitCode = 1;
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
            case WAIT_OBJECT_0:
                wsaSendBuffer.len = strlen(m_pSendBuffer);
                iResult = WSASend(sock, &wsaSendBuffer, 1, &dwBytesSent, 0, NULL, NULL);
                m_pSendBuffer[0] = '\0';
                break;
            case WAIT_OBJECT_0 + 1:
                HandleReceive(pRecvBuffer, sizeof(pRecvBuffer));
                ZeroMemory(pRecvBuffer, sizeof(pRecvBuffer));
                iResult = WSARecv(sock, &wsaRecvBuffer, 1, &dwBytesReceived, &dwFlags, &wsaRecvOverlapped, NULL);
                if (iResult == 0 && dwBytesReceived == 0) bStop = TRUE;
                break;
            case WAIT_OBJECT_0 + 2:
                HandleCommand();
                break;
            case WAIT_OBJECT_0 + 3:
                bStop = TRUE;
                break;
            default:
                dwExitCode = 1;
                bStop = TRUE;
                break;
            }
        } while (!bStop);

    } while (0);


    if (sock != INVALID_SOCKET) closesocket(sock);
    
    if (dwExitCode > 0) InsertDataStatus(VPN_ST_ERROR); 
    else if (m_phase == PHASE_SHUTDOWN) InsertDataStatus(VPN_ST_DISCONNECTED);

    CLOSEHANDLE(wsaRecvOverlapped.hEvent);

    return dwExitCode;
}

void CManagementSession::HandleCommand()
{
    switch (GetCommand()) {
    case VPN_CMD_CONNECT:
        // handled in  main
        break;
    case VPN_CMD_DISCONNECT:
        m_phase = PHASE_SHUTDOWN;
        QueueSendMessage(CMD_SIGNAL_SIGTERM);
        QueueSendMessage(CMD_EXIT);
        SetEvent(m_hSendEvent);
        break;
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
    if (strcmp(szLine, MSG_LOGEND) == 0) {
        m_phase = PHASE_INIT;
        return;
    }

    InsertDataLog(szLine);
}

void CManagementSession::MessageHandlerState(LPSTR szLine)
{
    LPSTR szToken       = NULL;
    LPSTR szContext     = NULL;

    szToken = strtok_s(szLine, ",", &szContext);
    szToken = strtok_s(szContext, ",", &szContext);

    if (strcmp(szToken, STATE_CONNECTED) == 0) {
        szToken = strtok_s(szContext, ",", &szContext);
        if (strcmp(szToken, MSG_SUCCESS) == 0) {
            szToken = strtok_s(szContext, ",", &szContext);
            szToken = strtok_s(szContext, ",", &szContext);
            InsertDataStatus(VPN_ST_CONNECTED, szToken);
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
    // this isn't necessary yet

    //QueueSendMessage(DEF_OPENVPN_PASSWORD);
    //SetEvent(m_hSendEvent);
}

void CManagementSession::MessageHandlerSuccess(LPSTR szLine)
{
    while (*szLine == ' ') szLine++;

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
    while (*szLine == ' ') szLine++;

    InsertDataStatus(VPN_ST_ERROR);
}

void CManagementSession::InsertDataStatus(VPN_STATUS status, LPSTR szExternalIP)
{
    m_data.Lock();
    if (status == VPN_ST_CONNECTED) {
        if (szExternalIP) m_data.csExternalIP = szExternalIP;
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
    BYTECOUNT_T *pb = &m_data.bytecounts.data[m_data.bytecounts.uiIndex];
    pb->ullBytesIn = ullBytesIn;
    pb->ullBytesOut = ullBytesOut;
    GetSystemTimeAsFileTime(&pb->ftTimestamp);
    (++m_data.bytecounts.uiIndex) %= _countof(m_data.bytecounts.data);
    m_data.dwEvents |= EVENT_BYTECOUNT;
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
