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

	ENTER_CS(this->syncRoot)

	ErrorIf(this->requests.size() >= CModuleConfiguration::GetMaxPendingRequestsPerApplication(context->GetHttpContext()), ERROR_NOT_ENOUGH_QUOTA);
	this->requests.push(context);

	// increase the pending async opertation count; corresponding decrease happens either from CProtocolBridge::SendEmptyResponse or 
	// CProtocolBridge::FinalizeResponse, possibly after severl context switches
	context->IncreasePendingAsyncOperationCount();

	LEAVE_CS(this->syncRoot)

	return S_OK;
Error:
	return hr;
}

CNodeHttpStoredContext* CPendingRequestQueue::Peek()
{
	ENTER_CS(this->syncRoot)

	return this->requests.size() > 0 ? this->requests.front() : NULL;

	LEAVE_CS(this->syncRoot)
}

void CPendingRequestQueue::Pop()
{
	ENTER_CS(this->syncRoot)

	if (this->requests.size() > 0)
	{
		this->requests.pop();
	}

	LEAVE_CS(this->syncRoot)
}