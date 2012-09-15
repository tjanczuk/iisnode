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
	BOOL isClosing;
	STARTUPINFOW startupInfo;
	BOOL hasProcessExited;
	OVERLAPPED overlapped;
	CConnectionPool connectionPool;
	DWORD pid;

	static unsigned int WINAPI ProcessWatcher(void* arg);
	void OnProcessExited();
	HRESULT CreateStdHandles(IHttpContext* context);
	void FlushStdHandles();	

public:

	CNodeProcess(CNodeProcessManager* processManager, IHttpContext* context);
	~CNodeProcess();

	HRESULT Initialize(IHttpContext* context);
	CNodeProcessManager* GetProcessManager();
	HANDLE GetProcess();
	DWORD GetPID();
	LPCTSTR GetNamedPipeName();
	CConnectionPool* GetConnectionPool();
	HRESULT AcceptRequest(CNodeHttpStoredContext* context);
	void OnRequestCompleted(CNodeHttpStoredContext* context);	
	void SignalWhenDrained(HANDLE handle);
	char* TryGetLog(IHttpContext* context, DWORD* size);
	BOOL HasProcessExited();
	DWORD GetActiveRequestCount();
};

#endif