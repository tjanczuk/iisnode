#ifndef __CNODEAPPLICATION_H__
#define __CNODEAPPLICATION_H__

class CPendingRequestQueue;
class CNodeProcessManager;

class CNodeApplication
{
private:

	PWSTR scriptName;
	CPendingRequestQueue* pendingRequests;
	CNodeApplicationManager* applicationManager;
	CNodeProcessManager* processManager;

	void Cleanup();

public:

	CNodeApplication(CNodeApplicationManager* applicationManager);	
	~CNodeApplication();

	HRESULT Initialize(PCWSTR scriptName);
	PCWSTR GetScriptName();
	CNodeApplicationManager* GetApplicationManager();
	CPendingRequestQueue* GetPendingRequestQueue();
	HRESULT StartNewRequest(IHttpContext* context, IHttpEventProvider* pProvider);
};

#endif