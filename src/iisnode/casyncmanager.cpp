#include "precomp.h"

CAsyncManager::CAsyncManager()
	: threads(NULL), threadCount(0), completionPort(NULL)
{
}

CAsyncManager::~CAsyncManager()
{
	this->Terminate();
}

HRESULT CAsyncManager::Initialize()
{
	HRESULT hr;
	DWORD initializedThreads = 0;

	ErrorIf(NULL != this->threads, ERROR_INVALID_OPERATION);

	this->threadCount = CModuleConfiguration::GetAsyncCompletionThreadCount();

	ErrorIf(NULL == (this->completionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, this->threadCount)), GetLastError());		
	
	ErrorIf(NULL == (this->threads = new HANDLE[this->threadCount]), ERROR_NOT_ENOUGH_MEMORY);
	
	for (initializedThreads = 0; initializedThreads < this->threadCount; initializedThreads++)
	{
		ErrorIf((HANDLE)-1L == (this->threads[initializedThreads] = (HANDLE) _beginthreadex(NULL, 0, CAsyncManager::Worker, this, 0, NULL)), ERROR_NOT_ENOUGH_MEMORY);
	}

	return S_OK;
Error:

	this->threadCount = initializedThreads;
	this->Terminate();

	return hr;
}

HRESULT CAsyncManager::AddAsyncCompletionHandle(HANDLE handle)
{
	HRESULT hr;

	CheckNull(handle);
	ErrorIf(NULL == CreateIoCompletionPort(handle, this->completionPort, NULL, 0), GetLastError());

	return S_OK;
Error:
	return hr;
}

HRESULT CAsyncManager::Terminate()
{
	HRESULT hr;

	if (NULL != this->threads)
	{
		for (DWORD i = 0; i < this->threadCount; i++)
		{
			ErrorIf(0 == PostQueuedCompletionStatus(this->completionPort, 0, -1L, NULL), ERROR_OPERATION_ABORTED);
		}

		WaitForMultipleObjects(this->threadCount, this->threads, TRUE, INFINITE);
		for (DWORD i = 0; i < this->threadCount; i++)
		{
			CloseHandle(this->threads[i]);
		}

		delete this->threads;
		this->threads = NULL;
		this->threadCount = 0;
	}

	if (NULL != this->completionPort)
	{
		CloseHandle(this->completionPort);
		this->completionPort = NULL;
	}

	return S_OK;
Error:
	return hr;
}

unsigned int WINAPI CAsyncManager::Worker(void* arg)
{
	HANDLE completionPort = ((CAsyncManager*)arg)->completionPort;
	ASYNC_CONTEXT* ctx;
	DWORD bytesRead = 0;
	ULONG_PTR pKey = NULL;
	DWORD error;

	while (true) 
	{
		error = ERROR_SUCCESS;
		if (!GetQueuedCompletionStatus(completionPort, &bytesRead, &pKey, (LPOVERLAPPED*)&ctx, INFINITE))
		{
			error = GetLastError();
		}

		if (-1L == pKey)
		{
			break;
		}

		if (NULL != ctx && NULL != ctx->completionProcessor)
		{
			ctx->completionProcessor(error, bytesRead, (LPOVERLAPPED)&ctx);
		}
	}

	return 0;
}

