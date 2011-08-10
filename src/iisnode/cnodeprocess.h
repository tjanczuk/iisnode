#ifndef __CNODEPROCESS_H__
#define __CNODEPROCESS_H__

class CNodeProcessManager;
class CNodeHttpStoredContext;
class CActiveRequestPool;

class CNodeProcess
{
private:

	CNodeProcessManager* processManager;
	CActiveRequestPool activeRequestPool;
	TCHAR namedPipe[256];
	HANDLE process;
	DWORD maxConcurrentRequestsPerProcess;

public:

	CNodeProcess(CNodeProcessManager* processManager);
	~CNodeProcess();

	HRESULT Initialize();
	CNodeProcessManager* GetProcessManager();
	BOOL TryAcceptRequest(CNodeHttpStoredContext* context);
};

#endif