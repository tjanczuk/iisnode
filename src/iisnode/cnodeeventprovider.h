#ifndef __CNODEEVENTPROVIDER_H__
#define __CNODEEVENTPROVIDER_H__

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

public:

	CNodeEventProvider();
	~CNodeEventProvider();

	HRESULT Initialize();
	bool IsEnabled(UCHAR level);
	HRESULT Log(PCWSTR message, UCHAR level);

};

#endif