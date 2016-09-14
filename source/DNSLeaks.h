#pragma once


class CDNSLeaksPrevention
{
public:
    CDNSLeaksPrevention();
    ~CDNSLeaksPrevention();

    BOOL Enable(LPCTSTR szVPNAdapterIP = NULL);
    BOOL Disable();

private:
    BOOL m_bFirewalled = FALSE;
    BOOL m_bPrioritized = FALSE;
    BOOL m_bInitialized = FALSE;
    CString m_csAdapterIP;
    long m_lIPMetric = 0;

private:
    BOOL FirewallDNSServers(BOOL bEnable);
    BOOL PrioritizeVPNInterface(BOOL bEnable);
    BOOL PrioritizeVPNInterfaceWMI(BOOL bEnable, int & iInterfaceIndex);

    BSTR GetBlockedDNSServers();
};

