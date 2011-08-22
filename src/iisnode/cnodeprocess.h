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
	DWORD ordinal;
	STARTUPINFO startupInfo;

	static unsigned int WINAPI ProcessWatcher(void* arg);
	void OnProcessExited();
	HRESULT CreateStdHandles();
	void FlushStdHandles();	

public:

	CNodeProcess(CNodeProcessManager* processManager, DWORD ordinal);
	~CNodeProcess();

	HRESULT Initialize();
	CNodeProcessManager* GetProcessManager();
	LPCTSTR GetNamedPipeName();
	HRESULT AcceptRequest(CNodeHttpStoredContext* context);
	void OnRequestCompleted(CNodeHttpStoredContext* context);	
	HANDLE CreateDrainHandle();
};

#endif