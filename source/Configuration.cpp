
#include "stdafx.h"
#include "Utils.h"
#include "Configuration.h"

#define DEF_REG_OPENVPN_KEY         _T("Software\\OpenVPN")
#define DEF_REG_OPENVPNGUI_KEY      _T("Software\\OpenVPN-GUI")
#define DEF_REG_OPENVPN_PATH        _T("")
#define DEF_REG_OPENVPN_CONFDIR     _T("config_dir")
#define DEF_REG_OPENVPN_CONFEXT     _T("config_ext")
#define DEF_REG_OPENVPN_EXEPATH     _T("exe_path")
#define DEF_REG_OPENVPN_LOGDIR      _T("log_dir")

#define DEF_CONF_DIR                _T("%APPDATA%\\") DEF_COMPANY_NAME

#define DEF_CONF_PREVENTDNSLEAKS    _T("prevent_dns_leaks")
#define DEF_CONF_PREVENTDNSLEAKS_DEFAULT 1
#define DEF_CONF_PROFILES           _T("profile_dir")
#define DEF_CONF_PROFILES_DEFAULT   _T("profiles")
#define DEF_CONF_PORT               _T("port")
#define DEF_CONF_PORT_DEFAULT       43069
#define DEF_CONF_BYTECOUNT_INTERVAL _T("bytecount_interval")
#define DEF_CONF_BYTECOUNT_INTERVAL_DEFAULT 3
#define DEF_CONF_PASSWORD           _T("password")
#define DEF_CONF_PASSWORD_DEFAULT   _T("prdel")
#define DEF_CONF_LOGFILE            _T("log_file")
#define DEF_CONF_LOGFILE_DEFAULT    DEF_APP_NAME _T(".log")



CConfiguration::CConfiguration()
{
    m_bLoaded = FALSE;
    m_bLocked = FALSE;

    m_bPreventDNSLeaks = FALSE;
    m_uiPort         = 0;
    m_uiBytecountInterval = 0;

    m_Profiles.SetCount(DEF_MAX_PROFILES);

    InitializeCriticalSection(&m_crsMain);
}
CConfiguration::~CConfiguration()
{
   Save();

   DeleteCriticalSection(&m_crsMain);
}

BOOL CConfiguration::Load()
{
    BOOL bRet = FALSE;

    EnterCriticalSection(&m_crsMain);

    do {
        bRet = LoadOpenVPN();
        if (!bRet) break;

        bRet = LoadMain();
        if (!bRet) break;
    } while (0);

    m_bLoaded = bRet;

    LeaveCriticalSection(&m_crsMain);

    return bRet;
}

BOOL CConfiguration::LoadOpenVPN()
{
    BOOL bRet = TRUE;
    LSTATUS regStatus   = ERROR_SUCCESS;
    HKEY hKey           = NULL;
    WCHAR szBuffer[MAX_PATH];
    DWORD dwType        = REG_SZ;
    DWORD dwBufferSize  = 0;

    do {
        regStatus = RegOpenKeyEx(HKEY_LOCAL_MACHINE, DEF_REG_OPENVPN_KEY, 0, KEY_READ | KEY_WOW64_64KEY, &hKey);
        if (regStatus != ERROR_SUCCESS) {
            regStatus = RegOpenKeyEx(HKEY_LOCAL_MACHINE, DEF_REG_OPENVPNGUI_KEY, 0, KEY_READ | KEY_WOW64_64KEY, &hKey);
            if (regStatus != ERROR_SUCCESS) { bRet = FALSE; break; };
        }

        dwBufferSize = sizeof(szBuffer);
        regStatus = RegQueryValueEx(hKey, DEF_REG_OPENVPN_PATH, NULL, &dwType, (LPBYTE)szBuffer, &dwBufferSize);
        if (regStatus != ERROR_SUCCESS) { bRet = FALSE; break; };
        m_csOVPNPath = szBuffer;

        dwBufferSize = sizeof(szBuffer);
        regStatus = RegQueryValueEx(hKey, DEF_REG_OPENVPN_CONFDIR, NULL, &dwType, (LPBYTE)szBuffer, &dwBufferSize);
        if (regStatus != ERROR_SUCCESS) { bRet = FALSE; break; };
        m_csOVPNConfigDir = szBuffer;

        dwBufferSize = sizeof(szBuffer);
        regStatus = RegQueryValueEx(hKey, DEF_REG_OPENVPN_CONFEXT, NULL, &dwType, (LPBYTE)szBuffer, &dwBufferSize);
        if (regStatus != ERROR_SUCCESS) { bRet = FALSE; break; };
        m_csOVPNConfigExt = szBuffer;

        dwBufferSize = sizeof(szBuffer);
        regStatus = RegQueryValueEx(hKey, DEF_REG_OPENVPN_EXEPATH, NULL, &dwType, (LPBYTE)szBuffer, &dwBufferSize);
        if (regStatus != ERROR_SUCCESS) { bRet = FALSE; break; };
        m_csOVPNExePath = szBuffer;

        dwBufferSize = sizeof(szBuffer);
        regStatus = RegQueryValueEx(hKey, DEF_REG_OPENVPN_LOGDIR, NULL, &dwType, (LPBYTE)szBuffer, &dwBufferSize);
        if (regStatus != ERROR_SUCCESS) { bRet = FALSE; break; };
        m_csOVPNLogDir = szBuffer;

        LPTSTR szPath = m_csOVPNExePath.GetBuffer();
        LPTSTR szSlash = _tcsrchr(szPath, _T('\\'));
        if (szSlash) {
            *szSlash = _T('\0');
            m_csOVPNBinDir = szPath;
            m_csOVPNExeName = szSlash + 1;
            *szSlash = _T('\\');
        }
        else bRet = FALSE;
        m_csOVPNExePath.ReleaseBuffer();
    } while (0);

    if (hKey) RegCloseKey(hKey);

    return bRet;
}

BOOL CConfiguration::LoadMain()
{
    BOOL bRet = FALSE;

    do {
        LPTSTR szBuffer = m_csBinDir.GetBuffer(MAX_PATH);

        if (GetModuleFileName(NULL, szBuffer, MAX_PATH) > 0) {
            LPTSTR szSlash = _tcsrchr(szBuffer, _T('\\'));
            if (szSlash) *szSlash = _T('\0');
        }
        m_csBinDir.ReleaseBuffer();

#ifdef USE_CONFIG_DIR
        if (!ExpandEnvironmentStrings(DEF_CONF_DIR, m_csConfigDir.GetBuffer(MAX_PATH), MAX_PATH)) break;
        m_csConfigDir.ReleaseBuffer();
#else
        m_csConfigDir = m_csBinDir;
#endif

        if (GetFileAttributes(m_csConfigDir) == INVALID_FILE_ATTRIBUTES) {
            if (!CreateDirectory(m_csConfigDir, NULL)) break;
        }

        m_csIniFile.Format(_T("%s\\%s.ini"), m_csConfigDir, DEF_APP_NAME);

        m_bPreventDNSLeaks = GetPrivateProfileInt(DEF_APP_NAME, DEF_CONF_PREVENTDNSLEAKS, DEF_CONF_PREVENTDNSLEAKS_DEFAULT, m_csIniFile);
        GetPrivateProfileString(DEF_APP_NAME, DEF_CONF_PROFILES, DEF_CONF_PROFILES_DEFAULT, m_csProfilesDir.GetBuffer(MAX_PATH), MAX_PATH, m_csIniFile);
        m_csProfilesDir.ReleaseBuffer();
        m_uiPort = GetPrivateProfileInt(DEF_APP_NAME, DEF_CONF_PORT, DEF_CONF_PORT_DEFAULT, m_csIniFile);
        m_uiBytecountInterval = GetPrivateProfileInt(DEF_APP_NAME, DEF_CONF_BYTECOUNT_INTERVAL, DEF_CONF_BYTECOUNT_INTERVAL_DEFAULT, m_csIniFile);
        GetPrivateProfileString(DEF_APP_NAME, DEF_CONF_PASSWORD, DEF_CONF_PASSWORD_DEFAULT, m_csPassword.GetBuffer(64), 64, m_csIniFile);
        m_csPassword.ReleaseBuffer();
        GetPrivateProfileString(DEF_APP_NAME, DEF_CONF_LOGFILE, DEF_CONF_LOGFILE_DEFAULT, m_csLogFile.GetBuffer(MAX_PATH), MAX_PATH, m_csIniFile);
        m_csLogFile.ReleaseBuffer();

        CUtils::ConvertRelativeToFullPath(m_csProfilesDir, m_csConfigDir);
        CUtils::ConvertRelativeToFullPath(m_csLogFile, m_csConfigDir);

        if (GetFileAttributes(m_csProfilesDir) == INVALID_FILE_ATTRIBUTES) {
            if (!CreateDirectory(m_csProfilesDir, NULL)) break;
        }

        bRet = TRUE;
    } while (0);

    return bRet;
}

BOOL CConfiguration::Save()
{
    BOOL bRet = FALSE;
    TCHAR szValue[32];

    EnterCriticalSection(&m_crsMain);

    do {
        if (!m_bLoaded) { bRet = TRUE; break; }

        _stprintf_s(szValue, _countof(szValue), _T("%d"), m_bPreventDNSLeaks);
        if (!WritePrivateProfileString(DEF_APP_NAME, DEF_CONF_PREVENTDNSLEAKS, szValue, m_csIniFile)) break;
        CUtils::ConvertFullToRelativePath(m_csProfilesDir, m_csConfigDir);
        if (!WritePrivateProfileString(DEF_APP_NAME, DEF_CONF_PROFILES, m_csProfilesDir, m_csIniFile)) break;
        _stprintf_s(szValue, _countof(szValue), _T("%d"), m_uiPort);
        if (!WritePrivateProfileString(DEF_APP_NAME, DEF_CONF_PORT, szValue, m_csIniFile)) break;
        _stprintf_s(szValue, _countof(szValue), _T("%d"), m_uiBytecountInterval);
        if (!WritePrivateProfileString(DEF_APP_NAME, DEF_CONF_BYTECOUNT_INTERVAL, szValue, m_csIniFile)) break;
        if (!WritePrivateProfileString(DEF_APP_NAME, DEF_CONF_PASSWORD, m_csPassword, m_csIniFile)) break;
        CUtils::ConvertFullToRelativePath(m_csLogFile, m_csConfigDir);
        if (!WritePrivateProfileString(DEF_APP_NAME, DEF_CONF_LOGFILE, m_csLogFile, m_csIniFile)) break;

        bRet = TRUE;
    } while (0);

    LeaveCriticalSection(&m_crsMain);

    return bRet;
}

UINT CConfiguration::ReloadProfiles()
{
    UINT uProfileCount          = 0;
    WIN32_FIND_DATA findData    = {0};
    HANDLE hFind                = INVALID_HANDLE_VALUE;
    PTSTR szExtension           = NULL;
    CString csSearch;
    VPN_PROFILE_T profile;

    EnterCriticalSection(&m_crsMain);

    do {
        m_Profiles.RemoveAll();

        if (!m_bLoaded) break;

        csSearch.Format(_T("%s\\*.%s"), m_csProfilesDir, m_csOVPNConfigExt);
        hFind = FindFirstFile(csSearch, &findData);
        if (hFind == INVALID_HANDLE_VALUE) break;

        do {
            profile.csConfPath.Format(_T("%s\\%s"), m_csProfilesDir, findData.cFileName);
            szExtension = _tcsrchr(findData.cFileName, _T('.'));
            if (szExtension) *szExtension = '\0';
            profile.csName = findData.cFileName;
            m_Profiles.Add(profile);
            uProfileCount++;
        } while (FindNextFile(hFind, &findData) && uProfileCount < DEF_MAX_PROFILES);

        FindClose(hFind);

    } while (0);

    LeaveCriticalSection(&m_crsMain);

    return uProfileCount;
}

BOOL CConfiguration::AddProfile(LPCTSTR szConfPath)
{
    BOOL bRet = FALSE;
    LPCTSTR szFileName = NULL;
    TCHAR szDestPath[MAX_PATH];

    do {
        szFileName = _tcsrchr(szConfPath, _T('\\'));
        if (!szFileName) break;

        _stprintf_s(szDestPath, _countof(szDestPath), _T("%s%s"), m_csProfilesDir, szFileName);

        bRet = CopyFile(szConfPath, szDestPath, FALSE);
    } while (0);

    return bRet;
}

BOOL CConfiguration::RemoveProfile(UINT uiIndex)
{
    BOOL bRet = FALSE;

    EnterCriticalSection(&m_crsMain);
    if (uiIndex >= 0 && uiIndex < m_Profiles.GetCount()) {
        bRet = DeleteFile(m_Profiles[uiIndex].csConfPath);
    }
    LeaveCriticalSection(&m_crsMain);
    
    return bRet;
}

BOOL CConfiguration::GetProfile(UINT uIndex, VPN_PROFILE_T &profile)
{
    BOOL bRet = FALSE;

    EnterCriticalSection(&m_crsMain);
    if (m_Profiles.GetCount() > uIndex) {
        profile = m_Profiles[uIndex];
        bRet = TRUE;
    }
    LeaveCriticalSection(&m_crsMain);

    return bRet;
}
