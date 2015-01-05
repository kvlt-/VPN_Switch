#include "stdafx.h"
#include <netfw.h>
#include <IPHlpApi.h>
#include "DNSLeaks.h"

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "iphlpapi.lib")

#define ADAPTERS_INFO_BUFFER_SIZE   16384

#define DNSLEAKS_RULE_NAME          L"VPN_Switch: DNS leak block"
#define DNSLEAKS_RULE_DESC          L"Rule added by VPN_Switch"
#define DNSLEAKS_RULE_GROUP         L""
#define DNSLEAKS_RULE_PROTOCOL      L"UDP"
#define DNSLEAKS_RULE_PORTS         L"53"
#define OPENVPN_ADAPTER_DESC        L"TAP-Windows Adapter V9"


CDNSLeaks::CDNSLeaks()
{
    m_bPreventing = FALSE;
    m_method = CDNSLeaks::METHOD_NONE;
}


CDNSLeaks::~CDNSLeaks()
{
}


BOOL CDNSLeaks::Prevent(BOOL bEnable, BOOL bForce)
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

    if (!bForce && m_bPreventing == bEnable) return TRUE;

    do {
        hrComInit = CoInitializeEx(0, COINIT_APARTMENTTHREADED);
        if (FAILED(hrComInit) && hrComInit != RPC_E_CHANGED_MODE) break;

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

        m_bPreventing = bEnable;
        bRet = TRUE;
    } while (0);

    if (bsBlockedServers) SysFreeString(bsBlockedServers);
    if (pNetFwPolicy2) pNetFwPolicy2->Release();
    if (SUCCEEDED(hrComInit)) CoUninitialize();

    return bRet;
}

#define GAA_ONLYDNS_FLAGS (GAA_FLAG_SKIP_UNICAST | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_FRIENDLY_NAME)

BSTR CDNSLeaks::GetBlockedDNSServers()
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

        ulRet = GetAdaptersAddresses(AF_UNSPEC, GAA_ONLYDNS_FLAGS, NULL, pAdapters, &ulSize);
        if (ulRet == ERROR_BUFFER_OVERFLOW) {
            PIP_ADAPTER_ADDRESSES pNewAdapters = (PIP_ADAPTER_ADDRESSES)realloc(pAdapters, ulSize);
            if (!pNewAdapters) break;
            pAdapters = pNewAdapters;
            ulRet = GetAdaptersAddresses(AF_UNSPEC, GAA_ONLYDNS_FLAGS, NULL, pAdapters, &ulSize);
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

                if (WSAAddressToString(pRestricted->Address.lpSockaddr, pRestricted->Address.iSockaddrLength, NULL, szTmp, &dwTmpSize) != 0) continue;
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
