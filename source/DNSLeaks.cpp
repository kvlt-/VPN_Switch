#include "stdafx.h"
#include <netfw.h>
#include <IPHlpApi.h>
#include <WbemIdl.h>
#include "DNSLeaks.h"

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "wbemuuid.lib")

#define ADAPTERS_INFO_BUFFER_SIZE   16384

#define DNSLEAKS_RULE_NAME          L"VPN_Switch: DNS leak block"
#define DNSLEAKS_RULE_DESC          L"Rule added by VPN_Switch"
#define DNSLEAKS_RULE_GROUP         L""
#define DNSLEAKS_RULE_PROTOCOL      L"UDP"
#define DNSLEAKS_RULE_PORTS         L"53"
#define OPENVPN_ADAPTER_DESC        L"TAP-Windows Adapter V9"
#define OPENVPN_ADAPTER_METRIC      3L


CDNSLeaksPrevention::CDNSLeaksPrevention()
{
}


CDNSLeaksPrevention::~CDNSLeaksPrevention()
{
    Disable();
}

BOOL CDNSLeaksPrevention::Enable(LPCTSTR szVPNAdapterIP)
{
    m_csAdapterIP = szVPNAdapterIP ? szVPNAdapterIP : _T("");

    if (!m_bFirewalled) {
        m_bFirewalled = FirewallDNSServers(TRUE);

        if (m_bFirewalled && !m_bPrioritized) {
            m_bPrioritized = PrioritizeVPNInterface(TRUE);
        }
    }

    return m_bFirewalled;
}

BOOL CDNSLeaksPrevention::Disable()
{
    if (m_bFirewalled) {
        m_bFirewalled = !FirewallDNSServers(FALSE);

        if (m_bPrioritized) {
            m_bPrioritized = !PrioritizeVPNInterface(FALSE);
        }
    }

    m_csAdapterIP.Empty();

    return !m_bFirewalled;
}

BOOL CDNSLeaksPrevention::FirewallDNSServers(BOOL bEnable)
{
    BOOL bRet = FALSE;
    HRESULT hrComInit = S_OK;
    HRESULT hr = S_OK;
    INetFwPolicy2 *pNetFwPolicy2 = NULL;
    INetFwRules *pFwRules = NULL;
    INetFwRule *pFwRule = NULL;
    INetFwRule *pFwNewRule = NULL;
    long lProfiles = 0;
    long lRulesCount = 0;
    ULONG ulFetchedCount = 0;
    IUnknown *pEnumerator = NULL;
    IEnumVARIANT* pEnumVar = NULL;
    BSTR bsName = NULL;
    CComVariant var;
    BSTR bsBlockedServers = NULL;
    BOOL bFound = FALSE;


    do {
        hrComInit = CoInitializeEx(0, COINIT_APARTMENTTHREADED);
        if (FAILED(hrComInit) && hrComInit != RPC_E_CHANGED_MODE) break;

        if (!m_bInitialized) {
            hr = CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);
            if (FAILED(hr)) break;
            m_bInitialized = TRUE;
        }

        hr = CoCreateInstance(__uuidof(NetFwPolicy2), NULL, CLSCTX_INPROC_SERVER, _uuidof(INetFwPolicy2), (void**)&pNetFwPolicy2);
        if (FAILED(hr)) break;

        // check if FW is enabled
        if (bEnable) {
            hr = pNetFwPolicy2->get_CurrentProfileTypes(&lProfiles);
            if (FAILED(hr)) break;

            BOOL bFirewallEnabled = (lProfiles != 0);
            if (bFirewallEnabled) {
                VARIANT_BOOL bProfileEnabled = FALSE;
                if (lProfiles & NET_FW_PROFILE2_DOMAIN) {
                    pNetFwPolicy2->get_FirewallEnabled(NET_FW_PROFILE2_DOMAIN, &bProfileEnabled);
                    bFirewallEnabled &= bProfileEnabled;
                }
                if (lProfiles & NET_FW_PROFILE2_PRIVATE) {
                    pNetFwPolicy2->get_FirewallEnabled(NET_FW_PROFILE2_PRIVATE, &bProfileEnabled);
                    bFirewallEnabled &= bProfileEnabled;
                }
                if (lProfiles & NET_FW_PROFILE2_PUBLIC) {
                    pNetFwPolicy2->get_FirewallEnabled(NET_FW_PROFILE2_PUBLIC, &bProfileEnabled);
                    bFirewallEnabled &= bProfileEnabled;
                }
            } 
            if (!bFirewallEnabled) break;
        }

        // get rules
        hr = pNetFwPolicy2->get_Rules(&pFwRules);
        if (FAILED(hr)) break;

        // get rules count
        hr = pFwRules->get_Count(&lRulesCount);
        if (FAILED(hr)) break;

        // get rules enumerator
        pFwRules->get__NewEnum(&pEnumerator);
        if (!pEnumerator) break;

        hr = pEnumerator->QueryInterface(__uuidof(IEnumVARIANT), (void **)&pEnumVar);

        // find rule with our name
        while (SUCCEEDED(hr) && hr != S_FALSE)
        {
            var.Clear();
            hr = pEnumVar->Next(1, &var, &ulFetchedCount);
            if (hr == S_FALSE) continue;

            hr = var.ChangeType(VT_DISPATCH);
            if (FAILED(hr)) continue;

            hr = (V_DISPATCH(&var))->QueryInterface(__uuidof(INetFwRule), reinterpret_cast<void**>(&pFwRule));
            if (FAILED(hr)) continue;

            if (SUCCEEDED(pFwRule->get_Name(&bsName))) {
                if (_tcscmp(bsName, DNSLEAKS_RULE_NAME) == 0) {
                    var.Clear();
                    bFound = TRUE;
                    break;
                }
            }
        }

        // remove existing rule
        if (bFound) {
            hr = pFwRules->Remove(bsName);
            if (FAILED(hr)) break;
        }

        // add new rule if requested
        if (bEnable) {
            bsBlockedServers = GetBlockedDNSServers();
            if (!bsBlockedServers) break;

            hr = CoCreateInstance(__uuidof(NetFwRule), NULL, CLSCTX_INPROC_SERVER, __uuidof(INetFwRule), (void**)&pFwNewRule);
            if (FAILED(hr)) break;

            pFwNewRule->put_Name(DNSLEAKS_RULE_NAME);
            pFwNewRule->put_Description(DNSLEAKS_RULE_DESC);
            pFwNewRule->put_Grouping(DNSLEAKS_RULE_GROUP);
            pFwNewRule->put_Enabled(VARIANT_TRUE);
            pFwNewRule->put_Protocol(NET_FW_IP_PROTOCOL_UDP);
            pFwNewRule->put_RemotePorts(DNSLEAKS_RULE_PORTS);
            pFwNewRule->put_RemoteAddresses(bsBlockedServers);
            pFwNewRule->put_Direction(NET_FW_RULE_DIR_OUT);
            pFwNewRule->put_Action(NET_FW_ACTION_BLOCK);
            pFwNewRule->put_Profiles(NET_FW_PROFILE2_ALL);

            hr = pFwRules->Add(pFwNewRule);
            if (FAILED(hr)) break;
        }

        bRet = TRUE;
    } while (0);

    if (bsBlockedServers) SysFreeString(bsBlockedServers);
    if (pNetFwPolicy2) pNetFwPolicy2->Release();
    if (SUCCEEDED(hrComInit)) CoUninitialize();

    return bRet;
}

BOOL CDNSLeaksPrevention::PrioritizeVPNInterface(BOOL bEnable)
{
    BOOL bRet = FALSE;

    do
    {
        int index = -1;

        bRet = PrioritizeVPNInterfaceWMI(bEnable, index);
        if (!bRet) break;

        STARTUPINFO si = { 0 };
        si.cb = sizeof(STARTUPINFO);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        CStringW cmdlineV4, cmdlineV6;
        cmdlineV4.Format(L"netsh interface ip set interface interface=%d metric=%d",
            index, bEnable ? OPENVPN_ADAPTER_METRIC : m_lIPMetric);
        cmdlineV6.Format(L"netsh interface ipv6 set interface interface=%d metric=%d",
            index, bEnable ? OPENVPN_ADAPTER_METRIC : m_lIPMetric);

        for (int i = 0; i <= 1; i++)
        {
            CStringW & cmd = (i == 0) ? cmdlineV4 : cmdlineV6;

            PROCESS_INFORMATION pi = { 0 };
            bRet = ::CreateProcessW(NULL, cmd.GetBuffer(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
            cmd.ReleaseBuffer();
            if (!bRet) {
                PrioritizeVPNInterfaceWMI(!bEnable, index);
                break;
            }

            if (pi.hThread) {
                CloseHandle(pi.hThread);
            }
            if (pi.hProcess) {
                WaitForSingleObject(pi.hProcess, INFINITE);
                CloseHandle(pi.hProcess);
            }
        }
    } while (0);

    return bRet;
}

// this only changes value in registry
// adapter needs restarting before the change actually applies
BOOL CDNSLeaksPrevention::PrioritizeVPNInterfaceWMI(BOOL bEnable, int & iInterfaceIndex)
{
    BOOL bRet = FALSE;
    HRESULT hrComInit = S_OK;
    HRESULT hr = S_OK;

    do {
        CComPtr<IWbemLocator> pLoc;
        CComPtr<IWbemServices> pSvc;
        CComPtr<IEnumWbemClassObject> pEnumerator;
        CComPtr<IWbemClassObject> pClass;
        CComPtr<IWbemClassObject> pMethod;
        CComPtr<IWbemClassObject> pIn;
        CComPtr<IWbemClassObject> pOut;

        if (m_csAdapterIP.IsEmpty()) break;

        hrComInit = CoInitializeEx(0, COINIT_MULTITHREADED);
        if (FAILED(hrComInit)) break;

        if (!m_bInitialized) {
            hr = CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);
            if (FAILED(hr)) break;
            m_bInitialized = TRUE;
        }

        hr = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID *)&pLoc);
        if (FAILED(hr)) break;

        hr = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), NULL, NULL, NULL, 0, NULL, NULL, &pSvc);
        if (FAILED(hr)) break;

        int index = -1;
        {
            hr = pSvc->ExecQuery(
                _bstr_t(L"WQL"),
                _bstr_t(L"SELECT * FROM Win32_NetworkAdapterConfiguration"),
                WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                NULL,
                &pEnumerator);
            if (FAILED(hr)) break;

            do {
                _variant_t iparg;
                CComPtr<IWbemClassObject> pConfig;

                ULONG ulReturnVal;
                hr = pEnumerator->Next(WBEM_INFINITE, 1, &pConfig, &ulReturnVal);
                if (hr == WBEM_S_FALSE) break;
                if (FAILED(hr)) break;
                
                hr = pConfig->Get(_bstr_t(L"IPAddress"), 0, &iparg, NULL, NULL);
                if (FAILED(hr)) break;
                
                if (iparg.vt != VT_NULL) {
                    BSTR *ip;

                    hr = SafeArrayAccessData(iparg.parray, (void**)&ip);
                    if (FAILED(hr)) break;

                    for (ULONG i = 0; i < iparg.parray->rgsabound[0].cElements; i++) {
                        if (wcscmp(m_csAdapterIP, ip[i]) == 0) {
                            _variant_t indexvar;
                            hr = pConfig->Get(_bstr_t(L"Index"), 0, &indexvar, NULL, NULL);
                            if (FAILED(hr)) break;

                            index = indexvar.lVal;

                            hr = pConfig->Get(_bstr_t(L"InterfaceIndex"), 0, &indexvar, NULL, NULL);
                            if (FAILED(hr)) break;

                            iInterfaceIndex = indexvar.lVal;

                            _variant_t metricvar;
                            hr = pConfig->Get(_bstr_t(L"IPConnectionMetric"), 0, &metricvar, NULL, NULL);
                            if (FAILED(hr)) break;

                            if (bEnable) m_lIPMetric = metricvar.lVal;

                            break;
                        }
                    }

                    SafeArrayUnaccessData(iparg.parray);
                }
            } while (index == -1);
        }

        if (index < 0) break;

        hr = pSvc->GetObject(_bstr_t(L"Win32_NetworkAdapterConfiguration"), 0, NULL, &pClass, NULL);
        if (FAILED(hr)) break;

        hr = pClass->GetMethod(_bstr_t(L"SetIPConnectionMetric"), 0, &pMethod, NULL);
        if (FAILED(hr)) break;

        hr = pMethod->SpawnInstance(0, &pIn);
        if (FAILED(hr)) break;

        hr = pMethod->SpawnInstance(0, &pOut);
        if (FAILED(hr)) break;

        _variant_t arg1(bEnable ? OPENVPN_ADAPTER_METRIC : m_lIPMetric, VT_I4);
        hr = pIn->Put(_bstr_t(L"IPConnectionMetric"), 0, &arg1, 0);
        if (FAILED(hr)) break;

        WCHAR search[128];
        swprintf_s(search, L"Win32_NetworkAdapterConfiguration.Index='%d'", index);

        hr = pSvc->ExecMethod(
            _bstr_t(search),
            _bstr_t(L"SetIPConnectionMetric"),
            0,
            NULL,
            pIn,
            &pOut.p,
            NULL);
        if (FAILED(hr)) break;

        _variant_t arg2(0L, VT_I4);
        hr = pOut->Get(_bstr_t(L"ReturnValue"), 0, &arg2, NULL, NULL);
        if (FAILED(hr)) break;
        if (arg2.lVal != 0) break;

        bRet = TRUE;
    } while (0);

    if (SUCCEEDED(hrComInit)) CoUninitialize();

    return bRet;
}


BSTR CDNSLeaksPrevention::GetBlockedDNSServers()
{
    BSTR bsRet = NULL;
    ULONG ulRet;
    ULONG ulSize = 0;
    PIP_ADAPTER_ADDRESSES pAdapters = NULL;
    PIP_ADAPTER_ADDRESSES pAdapter = NULL;
    PIP_ADAPTER_DNS_SERVER_ADDRESS pAllowedDNSAddress = NULL;
    PIP_ADAPTER_DNS_SERVER_ADDRESS pRestricted = NULL;
    PIP_ADAPTER_DNS_SERVER_ADDRESS pAllowed = NULL;
    BOOL bAdd;
    LPWSTR szPercent = NULL;
    WCHAR szTmp[128];
    DWORD dwTmpSize = _countof(szTmp);
    CStringW csBlocked;

    do {
        ulSize = ADAPTERS_INFO_BUFFER_SIZE;
        pAdapters = (PIP_ADAPTER_ADDRESSES)malloc(ulSize);
        if (!pAdapters) break;

        ULONG flags = GAA_FLAG_SKIP_UNICAST | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_FRIENDLY_NAME;

        ulRet = GetAdaptersAddresses(AF_UNSPEC, flags, NULL, pAdapters, &ulSize);
        if (ulRet == ERROR_BUFFER_OVERFLOW) {
            PIP_ADAPTER_ADDRESSES pNewAdapters = (PIP_ADAPTER_ADDRESSES)realloc(pAdapters, ulSize);
            if (!pNewAdapters) break;
            pAdapters = pNewAdapters;
            ulRet = GetAdaptersAddresses(AF_UNSPEC, flags, NULL, pAdapters, &ulSize);
        }
        if (ulRet != ERROR_SUCCESS) break;

        for (pAdapter = pAdapters; pAdapter; pAdapter = pAdapter->Next) {
            if (wcscmp(pAdapter->Description, OPENVPN_ADAPTER_DESC) == 0) {
                pAllowedDNSAddress = pAdapter->FirstDnsServerAddress;
                break;
            }
        }
        if (!pAllowedDNSAddress) break;

        for (pAdapter = pAdapters; pAdapter; pAdapter = pAdapter->Next) {
            if (pAdapter->FirstDnsServerAddress == pAllowedDNSAddress) continue;
            for (pRestricted = pAdapter->FirstDnsServerAddress; pRestricted; pRestricted = pRestricted->Next) {
                bAdd = TRUE;
                for (pAllowed = pAllowedDNSAddress; pAllowed; pAllowed = pAllowed->Next) {
                    if (pRestricted->Address.iSockaddrLength != pAllowed->Address.iSockaddrLength) continue;
                    if (memcmp(pRestricted->Address.lpSockaddr, pAllowed->Address.lpSockaddr, pRestricted->Address.iSockaddrLength) == 0) {
                        bAdd = FALSE;
                        break;
                    }
                }
                if (!bAdd) continue;

                if (WSAAddressToStringW(pRestricted->Address.lpSockaddr, pRestricted->Address.iSockaddrLength, NULL, szTmp, &dwTmpSize) != 0) continue;
                if (szPercent = wcschr(szTmp, L'%')) *szPercent = L'\0';
                if (csBlocked.Find(szTmp, 0) >= 0) continue;

                if (!csBlocked.IsEmpty()) csBlocked += L',';
                csBlocked += szTmp;
            }
        }

        if (csBlocked.IsEmpty()) break;

        bsRet = SysAllocString(csBlocked);
    } while (0);

    if (pAdapters) free(pAdapters);

    return bsRet;
}
