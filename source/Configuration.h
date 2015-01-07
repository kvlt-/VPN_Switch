
#pragma once


class CConfiguration
{
public:
    typedef struct _DATA_T
    {
        BOOL bPreventDNSLeaks;
        UINT uiPort;
        UINT uiBytecountInterval;
        CString csProfilesDir;
        CString csPassword;
        CString csLogFile;

        _DATA_T() { bPreventDNSLeaks = FALSE; uiPort = 0; uiBytecountInterval = 0; }
    } DATA_T;

public:
    CConfiguration();
    ~CConfiguration();

    BOOL Load();
    BOOL Save();

    BOOL AddProfile(LPCTSTR szConfPath);
    BOOL RemoveProfile(UINT uiIndex);
    BOOL GetProfile(UINT uiIndex, VPN_PROFILE_T &profile);
    DECLARE_GETTER_VAL(UINT, GetProfilesCount, m_profiles.GetCount());
    UINT ReloadProfiles();
    void RemoveAllProfiles();

    DECLARE_GETTER_VAL(CString, GetOVPNPath,            m_dataOpenVPN.csPath);
    DECLARE_GETTER_VAL(CString, GetOVPNConfigDir,       m_dataOpenVPN.csConfigDir);
    DECLARE_GETTER_VAL(CString, GetOVPNConfigExt,       m_dataOpenVPN.csConfigExt);
    DECLARE_GETTER_VAL(CString, GetOVPNExePath,         m_dataOpenVPN.csExePath);
    DECLARE_GETTER_VAL(CString, GetOVPNBinDir,          m_dataOpenVPN.csBinDir);
    DECLARE_GETTER_VAL(CString, GetOVPNExeName,         m_dataOpenVPN.csExeName);
    DECLARE_GETTER_VAL(CString, GetBinDir,              m_csBinDir);
    DECLARE_GETTER_VAL(CString, GetConfigDir,           m_csConfigDir);

    DECLARE_GETTER_REF_LOCK(CConfiguration::DATA_T, GetData, m_data);
    DECLARE_SETTER_REF_LOCK(CConfiguration::DATA_T, SetData, m_data);

    DECLARE_GETTER_VAL_LOCK(BOOL, IsLocked,             m_bLocked);
    DECLARE_SETTER_VAL_LOCK(BOOL, LockSettings,         m_bLocked);
    DECLARE_GETTER_VAL_LOCK(BOOL, GetPreventDNSLeaks,   m_data.bPreventDNSLeaks);
    DECLARE_SETTER_VAL_LOCK(BOOL, SetPreventDNSLeaks,   m_data.bPreventDNSLeaks);
    DECLARE_GETTER_VAL_LOCK(UINT, GetPort,              m_data.uiPort);
    DECLARE_SETTER_VAL_LOCK(UINT, SetPort,              m_data.uiPort);
    DECLARE_GETTER_VAL_LOCK(UINT, GetBytecountInterval, m_data.uiBytecountInterval);
    DECLARE_SETTER_VAL_LOCK(UINT, SetBytecountInterval, m_data.uiBytecountInterval);
    DECLARE_GETTER_VAL_LOCK(CString, GetProfilesDir,    m_data.csProfilesDir);
    DECLARE_GETTER_REF_LOCK(CString, GetProfilesDir,    m_data.csProfilesDir);
    DECLARE_SETTER_REF_LOCK(CString, SetProfilesDir,    m_data.csProfilesDir);
    DECLARE_GETTER_VAL_LOCK(CString, GetPassword,       m_data.csPassword);
    DECLARE_GETTER_REF_LOCK(CString, GetPassword,       m_data.csPassword);
    DECLARE_SETTER_REF_LOCK(CString, SetPassword,       m_data.csPassword);
    DECLARE_GETTER_VAL_LOCK(CString, GetLogFile,        m_data.csLogFile);
    DECLARE_GETTER_REF_LOCK(CString, GetLogFile,        m_data.csLogFile);
    DECLARE_SETTER_REF_LOCK(CString, SetLogFile,        m_data.csLogFile);



protected:
    typedef struct _DATA_OPENVPN_T
    {
        CString csPath;
        CString csConfigDir;
        CString csConfigExt;
        CString csExePath;
        CString csBinDir;
        CString csExeName;
    } DATA_OPENVPN_T;

protected:
    CWinApp *m_app;
    BOOL m_bLoaded;
    CString m_csConfFile;
    CRITICAL_SECTION m_crs;

    // getters only for these

    DATA_OPENVPN_T m_dataOpenVPN;
    CString m_csBinDir;
    CString m_csConfigDir;

    // getters/setters for these (locked by m_crsMain)

    BOOL m_bLocked;
    CAtlArray<VPN_PROFILE_T *> m_profiles;
    DATA_T m_data;

protected:
    BOOL LoadOpenVPN();
    BOOL LoadMain();

    BOOL LoadRegistryString(CRegKey &key, LPCTSTR szValue, CString &csDest, ULONG ulwMaxLength);

    void Lock()     { EnterCriticalSection(&m_crs); }
    void Unlock()   { LeaveCriticalSection(&m_crs); }
};