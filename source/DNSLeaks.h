#pragma once


class CDNSLeaks
{
public:
    CDNSLeaks();
    ~CDNSLeaks();

    BOOL Prevent(BOOL bEnable, BOOL bForce = FALSE);

protected:
    typedef enum
    {
        METHOD_NONE = 0,
        METHOD_FW,
        METHOD_ROUTES,
        METHOD_INTERFACES
    } METHOD;

protected:
    BOOL m_bPreventing;
    METHOD m_method;

protected:
    BSTR GetBlockedDNSServers();

};

