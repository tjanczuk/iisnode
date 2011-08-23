#ifndef __CNODEAPPLICATION_H__
#define __CNODEAPPLICATION_H__

class CPendingRequestQueue;
class CNodeProcessManager;
class CFileWatcher;

class CNodeApplication
{
private:

	PWSTR scriptName;
	CPendingRequestQueue* pendingRequests;
	CNodeApplicationManager* applicationManager;
	CNodeProcessManager* processManager;

	void Cleanup();
	static void OnScriptModified(PCWSTR fileName, void* data);

public:

	CNodeApplication(CNodeApplicationManager* applicationManager);	
	~CNodeApplication();

	HRESULT Initialize(PCWSTR scriptName, IHttpContext* context, CFileWatcher* fileWatcher);
	PCWSTR GetScriptName();
	CNodeApplicationManager* GetApplicationManager();
	CPendingRequestQueue* GetPendingRequestQueue();
	HRESULT Enqueue(IHttpContext* context, IHttpEventProvider* pProvider);
};

#endif