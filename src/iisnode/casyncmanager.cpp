#include "precomp.h"

extern RtlNtStatusToDosError pRtlNtStatusToDosError;

void ASYNC_CONTEXT::RunSynchronousContinuations()
{
    BOOL fCompletionPosted = FALSE;
	while (!fCompletionPosted && this->continueSynchronously)
	{
		// The continueSynchronously is used to unwind the call stack 
		// to avoid stack overflow in case of a synchronous IO completions
		this->continueSynchronously = FALSE;
		DWORD bytesCompleteted = this->bytesCompleteted;
		this->bytesCompleteted = 0;
		this->completionProcessor(S_OK, bytesCompleteted, (LPOVERLAPPED)this, &fCompletionPosted);
	}
}

CAsyncManager::CAsyncManager()
	: threads(NULL), threadCount(0), completionPort(NULL), timerThread(NULL), timerSignal(NULL)
{
}

CAsyncManager::~CAsyncManager()
{
	this->Terminate();
}

HRESULT CAsyncManager::Initialize(IHttpContext* context)
{
	HRESULT hr;
	DWORD initializedThreads = 0;

	ErrorIf(NULL != this->threads, ERROR_INVALID_OPERATION);

	this->threadCount = CModuleConfiguration::GetAsyncCompletionThreadCount(context);

	ErrorIf(NULL == (this->completionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, this->threadCount)), GetLastError());		
		
	ErrorIf(NULL == (this->timerSignal = CreateEvent(NULL, TRUE, FALSE, NULL)), ERROR_NOT_ENOUGH_MEMORY);
	ErrorIf((HANDLE)-1L == (this->timerThread = (HANDLE) _beginthreadex(NULL, 0, CAsyncManager::TimerWorker, this, 0, NULL)), ERROR_NOT_ENOUGH_MEMORY);

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

	if (NULL != this->timerThread)
	{
		SetEvent(this->timerSignal);
		WaitForSingleObject(this->timerThread, INFINITE);
		CloseHandle(this->timerThread);
		this->timerThread = NULL;
	}

	if (NULL != this->timerSignal)
	{
		CloseHandle(this->timerSignal);
		this->timerSignal = NULL;
	}

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

HRESULT CAsyncManager::SetTimer(ASYNC_CONTEXT* context, LARGE_INTEGER* dueTime)
{
	HRESULT hr;

	CheckNull(context);
	CheckNull(dueTime);	
	ErrorIf(NULL != context->timer, ERROR_INVALID_OPERATION);

	context->dueTime = *dueTime;
	context->completionPort = this->completionPort;
	ErrorIf(NULL == (context->timer = CreateWaitableTimer(NULL, TRUE, NULL)), GetLastError());
	ErrorIf(0 == QueueUserAPC(CAsyncManager::OnSetTimerApc, this->timerThread, (ULONG_PTR)context), GetLastError());

	return S_OK;
Error:
	return hr;
}

void CALLBACK CAsyncManager::OnSetTimerApc(ULONG_PTR dwParam)
{
	ASYNC_CONTEXT* ctx = (ASYNC_CONTEXT*)dwParam;
	SetWaitableTimer(ctx->timer, &ctx->dueTime, 0, CAsyncManager::OnTimer, ctx, FALSE);
}

unsigned int WINAPI CAsyncManager::TimerWorker(void* arg)
{
	CAsyncManager* async = (CAsyncManager*)arg;
	DWORD waitResult = WAIT_IO_COMPLETION;

	// Setting new timers and executing timer callbacks all happens in APCs on this thread.
	// The timerSignal is only signalled once when the process terminates and needs to clean up.
	
	while (WAIT_IO_COMPLETION == waitResult) {
		waitResult = WaitForSingleObjectEx(async->timerSignal, INFINITE, TRUE);
	}

	return 0;
}

unsigned int WINAPI CAsyncManager::Worker(void* arg)
{
	const int maxOverlappedEntries = 32;
	HANDLE completionPort = ((CAsyncManager*)arg)->completionPort;
	ASYNC_CONTEXT* ctx;
	DWORD error;
	OVERLAPPED_ENTRY entries[maxOverlappedEntries];
	ULONG entriesRemoved;

	while (true) 
	{
		error = ERROR_SUCCESS;
		RtlZeroMemory(&entries, maxOverlappedEntries * sizeof OVERLAPPED_ENTRY);

		if (GetQueuedCompletionStatusEx(completionPort, entries, maxOverlappedEntries, &entriesRemoved, INFINITE, TRUE))
		{
			OVERLAPPED_ENTRY* entry = entries;
			for (int i = 0; i < entriesRemoved; i++)
			{
                BOOL fCompletionPosted = FALSE;

				if (0L == entry->lpCompletionKey 
					&& NULL != (ctx = (ASYNC_CONTEXT*)entry->lpOverlapped) 
					&& NULL != ctx->completionProcessor) // regular IO completion - invoke custom processor
				{

					error = (entry->lpOverlapped->Internal == STATUS_SUCCESS) ? ERROR_SUCCESS 
						: pRtlNtStatusToDosError(entry->lpOverlapped->Internal);
					ctx = (ASYNC_CONTEXT*)entry->lpOverlapped;

					ctx->completionProcessor(
						(0 == entry->dwNumberOfBytesTransferred && ERROR_SUCCESS == error) ? ERROR_NO_DATA : error, 
						entry->dwNumberOfBytesTransferred, 
						(LPOVERLAPPED)ctx,
                        &fCompletionPosted);

                    if(!fCompletionPosted)
                    {
					    ctx->RunSynchronousContinuations();
                    }

					CNodeHttpStoredContext* storedCtx = (CNodeHttpStoredContext*)ctx->data;
					storedCtx->DereferenceNodeHttpStoredContext();
				}
				else if (-1L == entry->lpCompletionKey) // shutdown initiated from Terminate
				{
					return 0;
				}
				else if (NULL != entry->lpOverlapped)
				{
					if (-2L == entry->lpCompletionKey) // completion of an alertable wait state timer initialized from OnTimer
					{
						ctx = (ASYNC_CONTEXT*)entry->lpOverlapped;
						ctx->completionProcessor(S_OK, 0, (LPOVERLAPPED)ctx, &fCompletionPosted);
                        if(!fCompletionPosted)
                        {
						    ctx->RunSynchronousContinuations();
                        }

						CNodeHttpStoredContext* storedCtx = (CNodeHttpStoredContext*)ctx->data;
						storedCtx->DereferenceNodeHttpStoredContext();
					}
					else if (-3L == entry->lpCompletionKey) // continuation initiated form PostContinuation
					{
						((ContinuationCallback)entry->lpOverlapped)((void*)entry->dwNumberOfBytesTransferred);
					}
				}

				entry++;
			}
		}
		else if (ERROR_ABANDONED_WAIT_0 == GetLastError())
		{
			return ERROR_ABANDONED_WAIT_0;
		}
	}

	return 0;
}

void APIENTRY CAsyncManager::OnTimer(LPVOID lpArgToCompletionRoutine, DWORD dwTimerLowValue, DWORD dwTimerHighValue)
{
	ASYNC_CONTEXT* ctx = (ASYNC_CONTEXT*)lpArgToCompletionRoutine;

	CloseHandle(ctx->timer);
	ctx->timer = NULL;

	if (NULL != ctx && NULL != ctx->completionPort && NULL != ctx->completionProcessor)
	{
		PostQueuedCompletionStatus(ctx->completionPort, 0, -2L, (LPOVERLAPPED)ctx);
	}
}

HRESULT CAsyncManager::PostContinuation(ContinuationCallback continuation, void* data)
{
	HRESULT hr;

	CheckNull(continuation);
	ErrorIf(0 == PostQueuedCompletionStatus(this->completionPort, (DWORD)data, -3L, (LPOVERLAPPED)continuation), ERROR_OPERATION_ABORTED);

	return S_OK;
Error:
	return hr;
}
