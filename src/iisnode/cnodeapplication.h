#ifndef __CNODEAPPLICATION_H__
#define __CNODEAPPLICATION_H__

class CPendingRequestQueue;
class CNodeProcessManager;
class CFileWatcher;
class CNodeHttpStoredContext;
enum NodeDebugCommand;

class CNodeApplication
{
private:

	PWSTR scriptName;
	CPendingRequestQueue* pendingRequests;
	CNodeApplicationManager* applicationManager;
	CNodeProcessManager* processManager;
	CNodeApplication* peerApplication;
	BOOL isDebugger;
	NodeDebugCommand debugCommand;
	DWORD debugPort;

	void Cleanup();	

public:

	CNodeApplication(CNodeApplicationManager* applicationManager, BOOL isDebugger, NodeDebugCommand debugCommand, DWORD debugPort);	
	~CNodeApplication();

	HRESULT Initialize(PCWSTR scriptName, IHttpContext* context);
	PCWSTR GetScriptName();
	CNodeApplicationManager* GetApplicationManager();
	CPendingRequestQueue* GetPendingRequestQueue();
	HRESULT Enqueue(IHttpContext* context, IHttpEventProvider* pProvider, CNodeHttpStoredContext** ctx);
	CNodeApplication* GetPeerApplication();
	void SetPeerApplication(CNodeApplication* peerApplication);
	BOOL IsDebugger();
	BOOL IsDebuggee();
	BOOL IsDebugMode();
	NodeDebugCommand GetDebugCommand();
	HRESULT Recycle();
	DWORD GetDebugPort();
};

#endif