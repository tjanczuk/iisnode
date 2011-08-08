#ifndef __CNODEAPPLICATION_H__
#define __CNODEAPPLICATION_H__

class CPendingRequestQueue;

class CNodeApplication
{
private:

	PWSTR scriptName;
	CPendingRequestQueue* pendingRequests;
	CNodeApplicationManager* applicationManager;

public:

	CNodeApplication(CNodeApplicationManager* applicationManager);	
	~CNodeApplication();

	HRESULT Initialize(PCWSTR scriptName);
	PCWSTR GetScriptName();
	CNodeApplicationManager* GetApplicationManager();
	HRESULT StartNewRequest(IHttpContext* context, IHttpEventProvider* pProvider);
};

#endif