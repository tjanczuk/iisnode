#ifndef __CNODEEVENTPROVIDER_H__
#define __CNODEEVENTPROVIDER_H__

#include <httpserv.h>
#include <httptrace.h>

typedef ULONG (__stdcall *EventRegisterFunc)(
  _In_      LPCGUID ProviderId,
  _In_opt_  PENABLECALLBACK EnableCallback,
  _In_opt_  PVOID CallbackContext,
  _Out_     PREGHANDLE RegHandle
);

typedef ULONG (__stdcall *EventUnregisterFunc)(
  _In_  REGHANDLE RegHandle
);

typedef bool (__stdcall *EventProviderEnabledFunc)(
  __in  REGHANDLE RegHandle,
  __in  UCHAR Level,
  __in  ULONGLONG Keyword
);

typedef ULONG (__stdcall *EventWriteStringFunc)(
  __in  REGHANDLE RegHandle,
  __in  UCHAR Level,
  __in  ULONGLONG Keyword,
  __in  PCWSTR String
);

//
// Start of the new provider class WWWServerTraceProvider,
// GUID: {3a2a4e84-4c21-4981-ae10-3fda0d9b0f83}
// Description: IIS: WWW Server
//

class WWWServerTraceProvider
{
public:
    static
    LPCGUID
    GetProviderGuid( VOID )
    // return GUID for the current event class
    {
        static const GUID ProviderGuid = 
          {0x3a2a4e84,0x4c21,0x4981,{0xae,0x10,0x3f,0xda,0x0d,0x9b,0x0f,0x83}};
        return &ProviderGuid;
    };
    enum enumAreaFlags
    {
        // IISNODE Events
        IISNODE = 0x8000
    };
    static
    LPCWSTR
    TranslateEnumAreaFlagsToString( enum enumAreaFlags EnumValue)
    {
        switch( (DWORD) EnumValue )
        {
        case 0x8000: return L"IISNODE";
        }
        return NULL;
    };

    static
    BOOL
    CheckTracingEnabled(
        IHttpTraceContext * pHttpTraceContext,
        enumAreaFlags       AreaFlags,
        DWORD               dwVerbosity )
    {
        HRESULT                  hr;
        HTTP_TRACE_CONFIGURATION TraceConfig;
        TraceConfig.pProviderGuid = GetProviderGuid();
        hr = pHttpTraceContext->GetTraceConfiguration( &TraceConfig );
        if ( FAILED( hr )  || !TraceConfig.fProviderEnabled )
        {
            return FALSE;
        }
        if ( TraceConfig.dwVerbosity >= dwVerbosity && 
             (  TraceConfig.dwAreas == (DWORD) AreaFlags || 
               ( TraceConfig.dwAreas & (DWORD)AreaFlags ) == (DWORD)AreaFlags ) ) 
        { 
            return TRUE;
        } 
        return FALSE;
    };
};

class CNodeEventProvider
{
private:
    // {1040DFC4-61DB-484A-9530-584B2735F7F7}
    static const GUID providerId;

    REGHANDLE handle;
    HMODULE advapi;
    EventRegisterFunc eventRegister;
    EventUnregisterFunc eventUnregister;
    EventProviderEnabledFunc eventProviderEnabled;
    EventWriteStringFunc eventWriteString;

    static
    HRESULT
    RaiseEvent(
        IHttpTraceContext * pHttpTraceContext,
        PCWSTR message, 
        UCHAR level, 
        GUID* activityId = NULL
    )
    //
    // Raise IISNODE Event
    //
    {
        HTTP_TRACE_EVENT Event;
        Event.pProviderGuid = WWWServerTraceProvider::GetProviderGuid();
        Event.dwArea =  WWWServerTraceProvider::IISNODE;
        Event.pAreaGuid = GetAreaGuid();
        Event.dwEvent = 100;
        Event.pszEventName = L"IISNODE";
        Event.dwEventVersion = 1;
        Event.dwVerbosity = level;
        Event.cEventItems = 2;
        Event.pActivityGuid = NULL;
        Event.pRelatedActivityGuid = NULL;
        Event.dwTimeStamp = 0;
        Event.dwFlags = HTTP_TRACE_EVENT_FLAG_STATIC_DESCRIPTIVE_FIELDS;

        // pActivityGuid, pRelatedActivityGuid, Timestamp to be filled in by IIS

        HTTP_TRACE_EVENT_ITEM Items[ 2 ];
        Items[ 0 ].pszName = L"ActivityId";
        Items[ 0 ].dwDataType = HTTP_TRACE_TYPE_LPCGUID; // mof type (guid)
        Items[ 0 ].pbData = (PBYTE) activityId;
        Items[ 0 ].cbData  = 16;
        Items[ 0 ].pszDataDescription = NULL;
        Items[ 1 ].pszName = L"Message";
        Items[ 1 ].dwDataType = HTTP_TRACE_TYPE_LPCWSTR; // mof type (string)
        Items[ 1 ].pbData = (PBYTE) message;
        Items[ 1 ].cbData  = 
             ( Items[ 1 ].pbData == NULL )? 0 : ( sizeof(WCHAR) * (1 + (DWORD) wcslen( (PWSTR) Items[ 1 ].pbData  ) ) );
        Items[ 1 ].pszDataDescription = NULL;
        Event.pEventItems = Items;
        pHttpTraceContext->RaiseTraceEvent( &Event );
        return S_OK;
    };

    static
    LPCGUID
    GetAreaGuid( VOID )
    // return GUID for the current event class
    {
        // {c85d1b99-a120-417d-8ae7-e02a30300dea}
        static const GUID AreaGuid = 
          {0xc85d1b99,0xa120,0x417d,{0x8a,0xe7,0xe0,0x2a,0x30,0x30,0x0d,0xea}};
        return &AreaGuid;
    };

    bool IsEnabled(UCHAR level);

    bool IsEnabled(IHttpTraceContext * pHttpTraceContext, UCHAR level)
    {
        return WWWServerTraceProvider::CheckTracingEnabled( 
                                 pHttpTraceContext,
                                 WWWServerTraceProvider::IISNODE,
                                 level ); //Verbosity
    }

public:

    CNodeEventProvider();
    ~CNodeEventProvider();

    HRESULT Initialize();

    HRESULT Log(PCWSTR message, UCHAR level, GUID* activityId = NULL);
    HRESULT Log(IHttpContext *context, PCWSTR message, UCHAR level, GUID* activityId = NULL);

};

#endif