#ifndef __CASYNCMANAGER_H__
#define __CASYNCMANAGER_H__

typedef struct {
	OVERLAPPED overlapped; // this member must be first in the struct
	LPOVERLAPPED_COMPLETION_ROUTINE completionProcessor;	
	void* data;
	HANDLE timer;
	LARGE_INTEGER dueTime;
	BOOL synchronous; // TRUE means we are executing on a thread provided by IIS
	HANDLE completionPort;
} ASYNC_CONTEXT;

typedef void (*ContinuationCallback)(void* data);

class CAsyncManager
{
private:

	HANDLE completionPort;
	HANDLE* threads;
	int threadCount;
	HANDLE timerThread;
	HANDLE timerSignal;

	static unsigned int WINAPI Worker(void* arg);
	static unsigned int WINAPI TimerWorker(void* arg);
	static void APIENTRY OnTimer(LPVOID lpArgToCompletionRoutine, DWORD dwTimerLowValue, DWORD dwTimerHighValue);
	static void CALLBACK OnSetTimerApc(ULONG_PTR dwParam);

public:

	CAsyncManager();
	~CAsyncManager();

	HRESULT Initialize(IHttpContext* context);
	HRESULT AddAsyncCompletionHandle(HANDLE handle);
	HRESULT Terminate();
	HRESULT SetTimer(ASYNC_CONTEXT* context, LARGE_INTEGER* dueTime);
	HRESULT PostContinuation(ContinuationCallback continuation, void* data);
};

#endif