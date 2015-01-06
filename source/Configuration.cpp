
#include "stdafx.h"
#include "Utils.h"
#include "Configuration.h"

#define DEF_REG_OPENVPN_KEY         _T("Software\\OpenVPN")
#define DEF_REG_OPENVPNGUI_KEY      _T("Software\\OpenVPN-GUI")
#define DEF_REG_OPENVPN_PATH        _T("")
#define DEF_REG_OPENVPN_CONFDIR     _T("config_dir")
#define DEF_REG_OPENVPN_CONFEXT     _T("config_ext")
#define DEF_REG_OPENVPN_EXEPATH     _T("exe_path")

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

    InitializeCriticalSection(&m_crs);
}
CConfiguration::~CConfiguration()
{
   Save();
   RemoveAllProfiles();

   DeleteCriticalSection(&m_crs);
}

BOOL CConfiguration::Load()
{
    BOOL bRet = FALSE;

    Lock();

    do {
        bRet = LoadOpenVPN();
        if (!bRet) break;

        bRet = LoadMain();
        if (!bRet) break;
    } while (0);

    m_bLoaded = bRet;

    Unlock();

    return bRet;
}

BOOL CConfiguration::LoadOpenVPN()
{
    BOOL bRet = FALSE;
    CRegKey key;

    do {
        if (key.Open(HKEY_LOCAL_MACHINE, DEF_REG_OPENVPN_KEY, KEY_READ | KEY_WOW64_64KEY) != ERROR_SUCCESS) {
            key.Open(HKEY_LOCAL_MACHINE, DEF_REG_OPENVPNGUI_KEY, KEY_READ | KEY_WOW64_64KEY);
        }
        if (!key.m_hKey) break;

        if (!LoadRegistryString(key, DEF_REG_OPENVPN_PATH, m_dataOpenVPN.csPath, MAX_PATH)) break;
        if (!LoadRegistryString(key, DEF_REG_OPENVPN_CONFDIR, m_dataOpenVPN.csConfigDir, MAX_PATH)) break;
        if (!LoadRegistryString(key, DEF_REG_OPENVPN_CONFEXT, m_dataOpenVPN.csConfigExt, MAX_PATH)) break;
        if (!LoadRegistryString(key, DEF_REG_OPENVPN_EXEPATH, m_dataOpenVPN.csExePath, MAX_PATH)) break;

        LPTSTR szPath = m_dataOpenVPN.csExePath.GetBuffer();
        LPTSTR szSlash = _tcsrchr(szPath, _T('\\'));
        if (szSlash) {
            *szSlash = _T('\0');
            m_dataOpenVPN.csBinDir = szPath;
            m_dataOpenVPN.csExeName = szSlash + 1;
            *szSlash = _T('\\');
        }
        m_dataOpenVPN.csExePath.ReleaseBuffer();
        if (!m_dataOpenVPN.csExePath.GetLength()) break;

        bRet = TRUE;
    } while (0);

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

        m_data.bPreventDNSLeaks = GetPrivateProfileInt(DEF_APP_NAME, DEF_CONF_PREVENTDNSLEAKS, DEF_CONF_PREVENTDNSLEAKS_DEFAULT, m_csIniFile);
        GetPrivateProfileString(DEF_APP_NAME, DEF_CONF_PROFILES, DEF_CONF_PROFILES_DEFAULT, m_data.csProfilesDir.GetBuffer(MAX_PATH), MAX_PATH, m_csIniFile);
        m_data.csProfilesDir.ReleaseBuffer();
        m_data.uiPort = GetPrivateProfileInt(DEF_APP_NAME, DEF_CONF_PORT, DEF_CONF_PORT_DEFAULT, m_csIniFile);
        m_data.uiBytecountInterval = GetPrivateProfileInt(DEF_APP_NAME, DEF_CONF_BYTECOUNT_INTERVAL, DEF_CONF_BYTECOUNT_INTERVAL_DEFAULT, m_csIniFile);
        GetPrivateProfileString(DEF_APP_NAME, DEF_CONF_PASSWORD, DEF_CONF_PASSWORD_DEFAULT, m_data.csPassword.GetBuffer(64), 64, m_csIniFile);
        m_data.csPassword.ReleaseBuffer();
        GetPrivateProfileString(DEF_APP_NAME, DEF_CONF_LOGFILE, DEF_CONF_LOGFILE_DEFAULT, m_data.csLogFile.GetBuffer(MAX_PATH), MAX_PATH, m_csIniFile);
        m_data.csLogFile.ReleaseBuffer();

        CUtils::ConvertRelativeToFullPath(m_data.csProfilesDir, m_csConfigDir);
        CUtils::ConvertRelativeToFullPath(m_data.csLogFile, m_csConfigDir);

        if (GetFileAttributes(m_data.csProfilesDir) == INVALID_FILE_ATTRIBUTES) {
            if (!CreateDirectory(m_data.csProfilesDir, NULL)) break;
        }

        bRet = TRUE;
    } while (0);

    return bRet;
}

BOOL CConfiguration::LoadRegistryString(CRegKey &key, LPCTSTR szValue, CString &csDest, ULONG ulMaxLength)
{
    ULONG ulSize = ulMaxLength;
    LSTATUS ret = key.QueryStringValue(szValue, csDest.GetBuffer(ulSize), &ulSize);
    csDest.ReleaseBuffer();
    return ret == ERROR_SUCCESS;
}

BOOL CConfiguration::Save()
{
    BOOL bRet = FALSE;
    TCHAR szValue[32];

    Lock();

    do {
        if (!m_bLoaded) { bRet = TRUE; break; }

        _stprintf_s(szValue, _countof(szValue), _T("%d"), m_data.bPreventDNSLeaks);
        if (!WritePrivateProfileString(DEF_APP_NAME, DEF_CONF_PREVENTDNSLEAKS, szValue, m_csIniFile)) break;
        CUtils::ConvertFullToRelativePath(m_data.csProfilesDir, m_csConfigDir);
        if (!WritePrivateProfileString(DEF_APP_NAME, DEF_CONF_PROFILES, m_data.csProfilesDir, m_csIniFile)) break;
        _stprintf_s(szValue, _countof(szValue), _T("%d"), m_data.uiPort);
        if (!WritePrivateProfileString(DEF_APP_NAME, DEF_CONF_PORT, szValue, m_csIniFile)) break;
        _stprintf_s(szValue, _countof(szValue), _T("%d"), m_data.uiBytecountInterval);
        if (!WritePrivateProfileString(DEF_APP_NAME, DEF_CONF_BYTECOUNT_INTERVAL, szValue, m_csIniFile)) break;
        if (!WritePrivateProfileString(DEF_APP_NAME, DEF_CONF_PASSWORD, m_data.csPassword, m_csIniFile)) break;
        CUtils::ConvertFullToRelativePath(m_data.csLogFile, m_csConfigDir);
        if (!WritePrivateProfileString(DEF_APP_NAME, DEF_CONF_LOGFILE, m_data.csLogFile, m_csIniFile)) break;

        bRet = TRUE;
    } while (0);

    Unlock();

    return bRet;
}

BOOL CConfiguration::AddProfile(LPCTSTR szConfPath)
{
    BOOL bRet = FALSE;
    LPCTSTR szFileName = NULL;
    TCHAR szDestPath[MAX_PATH];

    do {
        szFileName = _tcsrchr(szConfPath, _T('\\'));
        if (!szFileName) break;

        _stprintf_s(szDestPath, _countof(szDestPath), _T("%s%s"), m_data.csProfilesDir, szFileName);

        bRet = CopyFile(szConfPath, szDestPath, FALSE);
    } while (0);

    return bRet;
}

BOOL CConfiguration::RemoveProfile(UINT uiIndex)
{
    BOOL bRet = FALSE;

    Lock();
    if (uiIndex >= 0 && uiIndex < m_profiles.GetCount()) {
        bRet = DeleteFile(m_profiles[uiIndex]->csConfPath);
    }
    Unlock();
    
    return bRet;
}

BOOL CConfiguration::GetProfile(UINT uIndex, VPN_PROFILE_T &profile)
{
    BOOL bRet = FALSE;

    Lock();
    if (m_profiles.GetCount() > uIndex) {
        profile = *m_profiles[uIndex];
        bRet = TRUE;
    }
    Unlock();

    return bRet;
}

UINT CConfiguration::ReloadProfiles()
{
    UINT uiCount               = 0;
    WIN32_FIND_DATA findData    = {0};
    HANDLE hFind                = INVALID_HANDLE_VALUE;
    PTSTR szExtension           = NULL;
    TCHAR szFind[MAX_PATH];
    VPN_PROFILE_T *pProfile = NULL;

    Lock();

    do {
        RemoveAllProfiles();
        m_profiles.SetCount(DEF_MAX_PROFILES);

        _stprintf_s(szFind, sizeof(szFind), _T("%s\\*.%s"), m_data.csProfilesDir, m_dataOpenVPN.csConfigExt);
        hFind = FindFirstFile(szFind, &findData);
        if (hFind == INVALID_HANDLE_VALUE) break;

        do {
            pProfile = new VPN_PROFILE_T;
            if (!pProfile) break;

            pProfile->csConfPath.Format(_T("%s\\%s"), m_data.csProfilesDir, findData.cFileName);
            szExtension = _tcsrchr(findData.cFileName, _T('.'));
            if (szExtension) *szExtension = '\0';
            pProfile->csName = findData.cFileName;
            m_profiles.Add(pProfile);
            uiCount++;
        } while (FindNextFile(hFind, &findData) && uiCount < DEF_MAX_PROFILES);

        FindClose(hFind);

    } while (0);

    if (!pProfile) while (uiCount > 0) delete m_profiles[--uiCount];
    m_profiles.SetCount(uiCount);

    Unlock();

    return uiCount;
}

void CConfiguration::RemoveAllProfiles()
{
    for (size_t i = 0; i < m_profiles.GetCount(); i++) delete m_profiles[i];
    m_profiles.RemoveAll();
}