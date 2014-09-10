#ifndef __CNODEPROCESSMANAGER_H__
#define __CNODEPROCESSMANAGER_H__

class CNodeProcess;
class CNodeHttpStoredContext;
class CNodeProcessManager;
class CNodeEventProvider;

class CNodeProcessManager
{
private:

	typedef struct {
		CNodeProcess** processes;
		DWORD count;
		CNodeProcess* process;
		CNodeProcessManager* processManager;
		BOOL disposeApplication;
		BOOL disposeProcess;
	} ProcessRecycleArgs;

	CNodeApplication* application;
	CNodeProcess** processes;
	DWORD processCount;
	unsigned int currentProcess;
	SRWLOCK srwlock;
	DWORD gracefulShutdownTimeout;
	BOOL isClosing;
	long refCount;
	CNodeEventProvider* eventProvider;
	HANDLE gracefulShutdownDrainHandle;
	long gracefulShutdownProcessCount;

	HRESULT AddProcess(int ordinal, IHttpContext* context);
	static unsigned int WINAPI GracefulShutdown(void* arg);
	void Cleanup();

public:

	CNodeProcessManager(CNodeApplication* application, IHttpContext* context);
	~CNodeProcessManager();

    HRESULT EmptyWorkingSet();
	CNodeApplication* GetApplication();
	HRESULT Initialize(IHttpContext* context);	
	HRESULT RecycleProcess(CNodeProcess* process);
	HRESULT Recycle();
	HRESULT Dispatch(CNodeHttpStoredContext* request);
	CNodeEventProvider* GetEventProvider();
	long AddRef();
	long DecRef();
	DWORD GetActiveRequestCount();
	DWORD GetProcessCount();
};

#endif