#include "precomp.h"

CPendingRequestQueue::CPendingRequestQueue()
	: count(0), list(NULL)
{	
}

HRESULT CPendingRequestQueue::Initialize()
{
	HRESULT hr;

	ErrorIf(NULL == (this->list = (PSLIST_HEADER)_aligned_malloc(sizeof(SLIST_HEADER), MEMORY_ALLOCATION_ALIGNMENT)), ERROR_NOT_ENOUGH_MEMORY);

	InitializeSListHead(this->list);

	return S_OK;
Error:
	return hr;
}

CPendingRequestQueue::~CPendingRequestQueue()
{
	PSLIST_ENTRY entry;

	if (this->list)
	{
		while (NULL != (entry = InterlockedPopEntrySList(this->list)))
		{
			CProtocolBridge::SendEmptyResponse(((PREQUEST_ENTRY)entry)->context, 503, "Service Unavailable", S_OK);
			_aligned_free(entry);
		}

		_aligned_free(this->list);
		this->list = NULL;
	}
}

BOOL CPendingRequestQueue::IsEmpty()
{
	return 0 == this->count;
}

HRESULT CPendingRequestQueue::Push(CNodeHttpStoredContext* context)
{
	HRESULT hr;
	PREQUEST_ENTRY entry = NULL;

	InterlockedIncrement(&this->count);
	CheckNull(context);
	ErrorIf(this->count >= CModuleConfiguration::GetMaxPendingRequestsPerApplication(context->GetHttpContext()), ERROR_NOT_ENOUGH_QUOTA);
	ErrorIf(NULL == (entry = (PREQUEST_ENTRY)_aligned_malloc(sizeof(REQUEST_ENTRY), MEMORY_ALLOCATION_ALIGNMENT)), ERROR_NOT_ENOUGH_MEMORY);	

	// increase the pending async opertation count; corresponding decrease happens either from CProtocolBridge::SendEmptyResponse or 
	// CProtocolBridge::FinalizeResponse, possibly after severl context switches
	context->IncreasePendingAsyncOperationCount();

	entry->context = context;
	InterlockedPushEntrySList(this->list, &(entry->listEntry));	

	return S_OK;
Error:

	InterlockedDecrement(&this->count);

	return hr;
}

CNodeHttpStoredContext* CPendingRequestQueue::Pop()
{
	CNodeHttpStoredContext* result = NULL;
	PREQUEST_ENTRY entry = (PREQUEST_ENTRY)InterlockedPopEntrySList(this->list);

	if (entry)
	{
		InterlockedDecrement(&this->count);
		result = entry->context;
		_aligned_free(entry);
	}

	return result;
}
