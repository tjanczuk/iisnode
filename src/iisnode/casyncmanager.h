#ifndef __CASYNCMANAGER_H__
#define __CASYNCMANAGER_H__

/*
* All async requests against Win32 APIs must use the ASYNC_CONTEXT structure as the OVERLAPPED parameter of the async call
* in order to use AsyncManager. 
* On async completion, the ASYNC_CONTEXT::completionProcessor will be called with following parameters:
* - context: the ASYNC_CONTEXT structure containing the OVERLAPPED property that caused the completion,
* - result: ERROR_SUCCESS when GetQueuedCompletionStatus returns true, or GetLastError() value otherwise,
* - bytesRead: as reported from GetQueuedCompletionStatus.
*/

typedef struct {
	OVERLAPPED overlapped; // this member must be first in the struct
	LPOVERLAPPED_COMPLETION_ROUTINE completionProcessor;	
	void* data;
	HANDLE timer;
	LARGE_INTEGER dueTime;
	BOOL asynchronous;
} ASYNC_CONTEXT;

class CAsyncManager
{
private:

	HANDLE completionPort;
	HANDLE* threads;
	int threadCount;

	static unsigned int WINAPI Worker(void* arg);
	static void APIENTRY OnTimer(LPVOID lpArgToCompletionRoutine, DWORD dwTimerLowValue, DWORD dwTimerHighValue);

public:

	CAsyncManager();
	~CAsyncManager();

	HRESULT Initialize();
	HRESULT AddAsyncCompletionHandle(HANDLE handle);
	HRESULT Terminate();
	HRESULT SetTimer(ASYNC_CONTEXT* context, LARGE_INTEGER* dueTime);
};

#endif