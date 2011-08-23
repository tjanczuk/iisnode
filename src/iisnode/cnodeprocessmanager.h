#ifndef __CNODEPROCESSMANAGER_H__
#define __CNODEPROCESSMANAGER_H__

class CNodeProcess;
class CNodeHttpStoredContext;

class CNodeProcessManager
{
private:

	typedef struct {
		CNodeProcess** processes;
		DWORD count;
		DWORD gracefulShutdownTimeout;
	} ProcessRecycleArgs;

	CNodeApplication* application;
	CNodeProcess** processes;
	DWORD processCount;
	DWORD maxProcessCount;
	DWORD currentProcess;
	CRITICAL_SECTION syncRoot;
	DWORD gracefulShutdownTimeout;

	HRESULT AddOneProcessCore(CNodeProcess** process, IHttpContext* context);
	HRESULT AddOneProcess(CNodeProcess** process, IHttpContext* context);
	BOOL TryRouteRequestToExistingProcess(CNodeHttpStoredContext* context);
	void TryDispatchOneRequestImpl();
	static unsigned int WINAPI GracefulShutdown(void* arg);

public:

	CNodeProcessManager(CNodeApplication* application, IHttpContext* context);
	~CNodeProcessManager();

	CNodeApplication* GetApplication();
	HRESULT Initialize();

	static void TryDispatchOneRequest(void* data);

	void RecycleProcess(CNodeProcess* process);
	void RecycleAllProcesses();
};

#endif