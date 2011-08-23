#include "precomp.h"

CActiveRequestPool::CActiveRequestPool()
	: drainHandle(NULL)
{
	InitializeCriticalSection(&this->syncRoot);
}

CActiveRequestPool::~CActiveRequestPool()
{
	DeleteCriticalSection(&this->syncRoot);
}

HRESULT CActiveRequestPool::Add(CNodeHttpStoredContext* context)
{
	HRESULT hr;

	CheckNull(context);

	ENTER_CS(this->syncRoot)

	ErrorIf(this->requests.size() >= CModuleConfiguration::GetMaxConcurrentRequestsPerProcess(context->GetHttpContext()), ERROR_NOT_ENOUGH_QUOTA);
	this->requests.push_back(context);

	LEAVE_CS(this->syncRoot)

	return S_OK;
Error:
	return hr;
}

HRESULT CActiveRequestPool::Remove(CNodeHttpStoredContext* context)
{
	HRESULT hr;
	BOOL signal = FALSE;

	CheckNull(context);

	ENTER_CS(this->syncRoot)

	this->requests.remove(context);

	if (NULL != this->drainHandle && this->requests.empty())
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

	if (this->requests.empty())
	{
		SetEvent(drainHandle);
	}
	else
	{
		this->drainHandle = drainHandle;
	}

	LEAVE_CS(this->syncRoot)
}