#include "precomp.h"

// {1040DFC4-61DB-484A-9530-584B2735F7F7}
const GUID CNodeEventProvider::providerId = { 0x1040dfc4, 0x61db, 0x484a, { 0x95, 0x30, 0x58, 0x4b, 0x27, 0x35, 0xf7, 0xf7 } };

CNodeEventProvider::CNodeEventProvider()
	: handle(0), advapi(NULL), eventRegister(NULL), eventUnregister(NULL), eventWriteString(NULL), eventProviderEnabled(NULL)
{
}

CNodeEventProvider::~CNodeEventProvider()
{
	if (this->advapi && this->eventUnregister && this->handle)
	{
		this->eventUnregister(this->handle);
		this->handle = 0;
	}

	if (this->advapi)
	{
		FreeLibrary(this->advapi);
		this->advapi = NULL;
	}
}

HRESULT CNodeEventProvider::Initialize()
{
	HRESULT hr;

	this->advapi = LoadLibrary("advapi32.dll");
	this->eventRegister = (EventRegisterFunc)GetProcAddress(this->advapi, "EventRegister");
	this->eventUnregister = (EventUnregisterFunc)GetProcAddress(this->advapi, "EventUnregister");
	this->eventWriteString = (EventWriteStringFunc)GetProcAddress(this->advapi, "EventWriteString");
	this->eventProviderEnabled = (EventProviderEnabledFunc)GetProcAddress(this->advapi, "EventProviderEnabled");

	if (this->eventRegister)
	{		
		CheckError(this->eventRegister(&providerId, NULL, NULL, &this->handle));
	}	

	return S_OK;
Error:

	if (this->advapi)
	{
		FreeLibrary(this->advapi);
		this->advapi = NULL;
	}

	return hr;
}

bool CNodeEventProvider::IsEnabled(UCHAR level)
{
	bool result = this->eventProviderEnabled ? this->eventProviderEnabled(this->handle, level, 0) : false;
	return result;
}

HRESULT CNodeEventProvider::Log(PCWSTR message, UCHAR level)
{
	HRESULT hr;

	CheckError(this->eventWriteString(this->handle, level, 0, message));

	return S_OK;
Error:
	return hr;
}
