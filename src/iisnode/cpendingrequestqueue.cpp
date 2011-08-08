#include "precomp.h"

CPendingRequestQueue::CPendingRequestQueue()
{
	InitializeCriticalSection(&this->syncRoot);
}

CPendingRequestQueue::~CPendingRequestQueue()
{
	DeleteCriticalSection(&this->syncRoot);
}

HRESULT CPendingRequestQueue::Push(CNodeHttpStoredContext* context)
{
	HRESULT hr;

	CheckNull(context);

	EnterCriticalSection(&this->syncRoot);

	ErrorIf(this->requests.size() >= CModuleConfiguration::GetMaxPendingRequestsPerApplication(), ERROR_PARAMETER_QUOTA_EXCEEDED);
	this->requests.push(context);

	LeaveCriticalSection(&this->syncRoot);

	return S_OK;
Error:
	return hr;
}