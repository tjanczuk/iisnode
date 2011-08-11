#include "precomp.h"

CPendingRequestQueue::CPendingRequestQueue()
{
	InitializeCriticalSection(&this->syncRoot);
}

CPendingRequestQueue::~CPendingRequestQueue()
{
	DeleteCriticalSection(&this->syncRoot);
}

BOOL CPendingRequestQueue::IsEmpty()
{
	return this->requests.empty();
}

HRESULT CPendingRequestQueue::Push(CNodeHttpStoredContext* context)
{
	HRESULT hr;

	CheckNull(context);

	EnterCriticalSection(&this->syncRoot);

	ErrorIf(this->requests.size() >= CModuleConfiguration::GetMaxPendingRequestsPerApplication(), ERROR_NOT_ENOUGH_QUOTA);
	this->requests.push(context);

	LeaveCriticalSection(&this->syncRoot);

	return S_OK;
Error:
	return hr;
}

CNodeHttpStoredContext* CPendingRequestQueue::Peek()
{
	EnterCriticalSection(&this->syncRoot);

	return this->requests.size() > 0 ? this->requests.front() : NULL;

	LeaveCriticalSection(&this->syncRoot);
}

void CPendingRequestQueue::Pop()
{
	EnterCriticalSection(&this->syncRoot);

	if (this->requests.size() > 0)
	{
		this->requests.pop();
	}

	LeaveCriticalSection(&this->syncRoot);
}