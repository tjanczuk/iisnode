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
	DWORD maxProcessCount;
	DWORD currentProcess;
	SRWLOCK srwlock;
	DWORD gracefulShutdownTimeout;
	BOOL isClosing;
	long refCount;
	CNodeEventProvider* eventProvider;

	HRESULT AddOneProcessCore(CNodeProcess** process, IHttpContext* context);
	HRESULT AddOneProcess(CNodeProcess** process, IHttpContext* context);
	BOOL TryRouteRequestToExistingProcess(CNodeHttpStoredContext* context);
	static unsigned int WINAPI GracefulShutdown(void* arg);

public:

	CNodeProcessManager(CNodeApplication* application, IHttpContext* context);
	~CNodeProcessManager();

	CNodeApplication* GetApplication();
	HRESULT Initialize(IHttpContext* context);	
	HRESULT RecycleProcess(CNodeProcess* process);
	HRESULT Recycle();
	HRESULT Dispatch(CNodeHttpStoredContext* request);
	CNodeEventProvider* GetEventProvider();
	long AddRef();
	long DecRef();
};

#endif