#include "precomp.h"

CActiveRequestPool::CActiveRequestPool()
	: drainHandle(NULL), requestCount(0)
{
	InitializeCriticalSection(&this->syncRoot);
}

CActiveRequestPool::~CActiveRequestPool()
{
	DeleteCriticalSection(&this->syncRoot);
}

DWORD CActiveRequestPool::GetRequestCount()
{
	return this->requestCount;
}

HRESULT CActiveRequestPool::Add(CNodeHttpStoredContext* context)
{
	HRESULT hr;

	CheckNull(context);

	ENTER_CS(this->syncRoot)
	
	ErrorIf(this->requestCount >= CModuleConfiguration::GetMaxConcurrentRequestsPerProcess(context->GetHttpContext()), ERROR_NOT_ENOUGH_QUOTA);
	this->requestCount++;

	LEAVE_CS(this->syncRoot)

	return S_OK;
Error:
	return hr;
}

HRESULT CActiveRequestPool::Remove()
{
	HRESULT hr;
	BOOL signal = FALSE;

	ENTER_CS(this->syncRoot)

	ErrorIf(this->requestCount == 0, ERROR_INVALID_OPERATION);

	this->requestCount--;

	if (NULL != this->drainHandle && 0 == this->requestCount)
	{
		signal = TRUE;
	}

	LEAVE_CS(this->syncRoot)

	if (signal)
	{
		SetEvent(this->drainHandle);
	}

	return S_OK;
Error:
	return hr;
}

void CActiveRequestPool::SignalWhenDrained(HANDLE drainHandle)
{
	ENTER_CS(this->syncRoot)

	if (0 == this->requestCount)
	{
		SetEvent(drainHandle);
	}
	else
	{
		this->drainHandle = drainHandle;
	}

	LEAVE_CS(this->syncRoot)
}