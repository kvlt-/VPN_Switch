
#pragma once


class CConfiguration
{
public:
    CConfiguration();
    ~CConfiguration();

    BOOL Load();
    BOOL Save();

    UINT ReloadProfiles();
    BOOL AddProfile(LPCTSTR szConfPath);
    BOOL RemoveProfile(UINT uiIndex);
    BOOL GetProfile(UINT uiIndex, VPN_PROFILE_T &profile);

    DECLARE_GETTER_REF(CString, GetOVPNPath, m_csOVPNPath);
    DECLARE_GETTER_REF(CString, GetOVPNConfigDir, m_csOVPNConfigDir);
    DECLARE_GETTER_REF(CString, GetOVPNConfigExt, m_csOVPNConfigExt);
    DECLARE_GETTER_REF(CString, GetOVPNExePath, m_csOVPNExePath);
    DECLARE_GETTER_REF(CString, GetOVPNBinDir, m_csOVPNBinDir);
    DECLARE_GETTER_REF(CString, GetOVPNExeName, m_csOVPNExeName);
    DECLARE_GETTER_REF(CString, GetOVPNLogDir, m_csOVPNLogDir);
    DECLARE_GETTER_REF(CString, GetBinDir, m_csBinDir);
    DECLARE_GETTER_REF(CString, GetConfigDir, m_csConfigDir);

    DECLARE_GETTER_VAL_LOCK(BOOL, IsLocked, m_bLocked);
    DECLARE_SETTER_VAL_LOCK(BOOL, LockSettings, m_bLocked);
    DECLARE_GETTER_VAL_LOCK(BOOL, GetPreventDNSLeaks, m_bPreventDNSLeaks);
    DECLARE_SETTER_REF_LOCK(BOOL, SetPreventDNSLeaks, m_bPreventDNSLeaks);
    DECLARE_GETTER_VAL_LOCK(UINT, GetPort, m_uiPort);
    DECLARE_SETTER_REF_LOCK(UINT, SetPort, m_uiPort);
    DECLARE_GETTER_VAL_LOCK(UINT, GetBytecountInterval, m_uiBytecountInterval);
    DECLARE_SETTER_REF_LOCK(UINT, SetBytecountInterval, m_uiBytecountInterval);
    DECLARE_GETTER_VAL_LOCK(CString, GetProfilesDir, m_csProfilesDir);
    DECLARE_GETTER_REF_LOCK(CString, GetProfilesDir, m_csProfilesDir);
    DECLARE_SETTER_REF_LOCK(CString, SetProfilesDir, m_csProfilesDir);
    DECLARE_GETTER_VAL_LOCK(CString, GetPassword, m_csPassword);
    DECLARE_GETTER_REF_LOCK(CString, GetPassword, m_csPassword);
    DECLARE_SETTER_REF_LOCK(CString, SetPassword, m_csPassword);
    DECLARE_GETTER_VAL_LOCK(CString, GetLogFile, m_csLogFile);
    DECLARE_GETTER_REF_LOCK(CString, GetLogFile, m_csLogFile);
    DECLARE_SETTER_REF_LOCK(CString, SetLogFile, m_csLogFile);



protected:
    typedef CAtlArray<VPN_PROFILE_T> VPN_PROFILE_ARRAY_T;

    BOOL m_bLoaded;
    CString m_csIniFile;
    CRITICAL_SECTION m_crsMain;

    // getters only for these

    CString m_csOVPNPath;
    CString m_csOVPNConfigDir;
    CString m_csOVPNConfigExt;
    CString m_csOVPNExePath;
    CString m_csOVPNBinDir;
    CString m_csOVPNExeName;
    CString m_csOVPNLogDir;
    CString m_csBinDir;
    CString m_csConfigDir;

    // getters/setters for these (locked by m_crsMain)

    BOOL m_bLocked;
    VPN_PROFILE_ARRAY_T m_Profiles;
    BOOL m_bPreventDNSLeaks;
    UINT m_uiPort;
    UINT m_uiBytecountInterval;
    CString m_csProfilesDir;
    CString m_csPassword;
    CString m_csLogFile;

protected:
    BOOL LoadOpenVPN();
    BOOL LoadMain();


};