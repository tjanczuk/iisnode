#ifndef __CNODEAPPLICATIONMANAGER_H__
#define __CNODEAPPLICATIONMANAGER_H__

class CNodeApplication;

class CNodeApplicationManager
{
private:

	typedef struct _NodeApplicationEntry {
		CNodeApplication* nodeApplication;
		struct _NodeApplicationEntry* next;
	} NodeApplicationEntry;

	IHttpServer* server;
	HTTP_MODULE_ID moduleId;
	NodeApplicationEntry* applications;
	CRITICAL_SECTION syncRoot;

	HRESULT GetOrCreateNodeApplication(IHttpContext* context, CNodeApplication** application);
	CNodeApplication* TryGetExistingNodeApplication(PCWSTR physicalPath);

public:

	CNodeApplicationManager(IHttpServer* server, HTTP_MODULE_ID moduleId); 
	~CNodeApplicationManager();

	IHttpServer* GetHttpServer();
	HTTP_MODULE_ID GetModuleId();
	HRESULT StartNewRequest(IHttpContext* context, IHttpEventProvider* pProvider);
};

#endif