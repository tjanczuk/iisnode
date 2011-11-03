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
	DWORD ordinal;
	STARTUPINFO startupInfo;
	BOOL loggingEnabled;
	DWORD logFlushInterval;
	LONGLONG maxLogSizeInBytes;
	BOOL hasProcessExited;

	static unsigned int WINAPI ProcessWatcher(void* arg);
	void OnProcessExited();
	HRESULT CreateStdHandles(IHttpContext* context);
	void FlushStdHandles();	

public:

	CNodeProcess(CNodeProcessManager* processManager, IHttpContext* context, DWORD ordinal);
	~CNodeProcess();

	HRESULT Initialize(IHttpContext* context);
	CNodeProcessManager* GetProcessManager();
	LPCTSTR GetNamedPipeName();
	HRESULT AcceptRequest(CNodeHttpStoredContext* context);
	void OnRequestCompleted(CNodeHttpStoredContext* context);	
	void SignalWhenDrained(HANDLE handle);
};

#endif