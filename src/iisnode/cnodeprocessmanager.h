#ifndef __CNODEPROCESSMANAGER_H__
#define __CNODEPROCESSMANAGER_H__

class CNodeProcess;
class CNodeHttpStoredContext;

class CNodeProcessManager
{
private:

	CNodeApplication* application;
	CNodeProcess** processes;
	DWORD processCount;
	DWORD maxProcessCount;
	DWORD currentProcess;
	CRITICAL_SECTION syncRoot;

	HRESULT AddOneProcess(CNodeProcess** process);
	BOOL TryRouteRequestToExistingProcess(CNodeHttpStoredContext* context);

public:

	CNodeProcessManager(CNodeApplication* application);
	~CNodeProcessManager();

	CNodeApplication* GetApplication();
	HRESULT Initialize();

	HRESULT TryDispatchOneRequest();
};

#endif