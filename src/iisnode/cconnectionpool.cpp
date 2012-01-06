#include "precomp.h"

CConnectionPool::CConnectionPool()
	: count(0), list(NULL)
{	
}

HRESULT CConnectionPool::Initialize(IHttpContext* ctx)
{
	HRESULT hr;

	ErrorIf(NULL == (this->list = (PSLIST_HEADER)_aligned_malloc(sizeof(SLIST_HEADER), MEMORY_ALLOCATION_ALIGNMENT)), ERROR_NOT_ENOUGH_MEMORY);
	this->maxPoolSize = CModuleConfiguration::GetMaxNamedPipeConnectionPoolSize(ctx);
	this->maxAge = CModuleConfiguration::GetMaxNamedPipePooledConnectionAge(ctx);

	InitializeSListHead(this->list);

	return S_OK;
Error:
	return hr;
}

CConnectionPool::~CConnectionPool()
{
	PSLIST_ENTRY entry;

	if (this->list)
	{
		while (NULL != (entry = InterlockedPopEntrySList(this->list)))
		{
			CloseHandle(((PCONNECTION_ENTRY)entry)->connection);
			_aligned_free(entry);
		}

		_aligned_free(this->list);
		this->list = NULL;
	}
}

HRESULT CConnectionPool::Return(HANDLE connection)
{
	HRESULT hr;
	PCONNECTION_ENTRY entry = NULL;
	
	if (this->count >= this->maxPoolSize)
	{
		CloseHandle(connection);
	}
	else
	{
		ErrorIf(NULL == (entry = (PCONNECTION_ENTRY)_aligned_malloc(sizeof(CONNECTION_ENTRY), MEMORY_ALLOCATION_ALIGNMENT)), ERROR_NOT_ENOUGH_MEMORY);	
		entry->connection = connection;
		entry->timestamp = GetTickCount();
		InterlockedPushEntrySList(this->list, &(entry->listEntry));	
		InterlockedIncrement(&this->count);
	}

	return S_OK;
Error:

	return hr;
}

HANDLE CConnectionPool::Take()
{
	HANDLE result = INVALID_HANDLE_VALUE;
	PCONNECTION_ENTRY entry;
	DWORD now = GetTickCount();

	while (NULL != (entry = (PCONNECTION_ENTRY)InterlockedPopEntrySList(this->list)))
	{
		InterlockedDecrement(&this->count);
		if ((now - entry->timestamp) < this->maxAge)
		{
			result = entry->connection;
		}
		else
		{
			CloseHandle(entry->connection);
		}

		_aligned_free(entry);

		if (INVALID_HANDLE_VALUE != result)
		{
			break;
		}
	}

	return result;
}