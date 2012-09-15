#ifndef __CNODEAPPLICATION_H__
#define __CNODEAPPLICATION_H__

class CNodeProcessManager;
class CFileWatcher;
class CNodeHttpStoredContext;
enum NodeDebugCommand;

class CNodeApplication
{
private:

	PWSTR scriptName;
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
	HRESULT Dispatch(IHttpContext* context, IHttpEventProvider* pProvider, CNodeHttpStoredContext** ctx);
	CNodeApplication* GetPeerApplication();
	void SetPeerApplication(CNodeApplication* peerApplication);
	BOOL IsDebugger();
	BOOL IsDebuggee();
	BOOL IsDebugMode();
	NodeDebugCommand GetDebugCommand();
	HRESULT Recycle();
	DWORD GetDebugPort();
	DWORD GetActiveRequestCount();
	DWORD GetProcessCount();
};

#endif