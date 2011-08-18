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
	HANDLE processWatcher;
	DWORD maxConcurrentRequestsPerProcess;
	unsigned int isClosing;

	static unsigned int WINAPI ProcessTerminationWatcher(void* arg);
	void OnProcessExited();

public:

	CNodeProcess(CNodeProcessManager* processManager);
	~CNodeProcess();

	HRESULT Initialize();
	CNodeProcessManager* GetProcessManager();
	LPCTSTR GetNamedPipeName();
	HRESULT AcceptRequest(CNodeHttpStoredContext* context);
	void OnRequestCompleted(CNodeHttpStoredContext* context);	
	HANDLE CreateDrainHandle();
};

#endif