#ifndef __CNODEAPPLICATIONMANAGER_H__
#define __CNODEAPPLICATIONMANAGER_H__

class CNodeApplication;
class CAsyncManager;
class CFileWatcher;
class CNodeEventProvider;
class CNodeHttpStoredContext;
enum NodeDebugCommand;

class CNodeApplicationManager
{
private:

	typedef struct _NodeApplicationEntry {
		CNodeApplication* nodeApplication;
		struct _NodeApplicationEntry* next;
	} NodeApplicationEntry;

	typedef struct _DebuggerFileEnumeratorParams {
		HRESULT hr;
		PWSTR physicalFile;
		DWORD physicalFileLength;
		DWORD physicalFileSize;
	} DebuggerFileEnumeratorParams;

	IHttpServer* server;
	HTTP_MODULE_ID moduleId;
	NodeApplicationEntry* applications;
	CRITICAL_SECTION syncRoot;
	CAsyncManager* asyncManager;
	HANDLE jobObject;
	BOOL breakAwayFromJobObject;
	CFileWatcher* fileWatcher;
	CNodeEventProvider* eventProvider;
	BOOL initialized;
	DWORD currentDebugPort;

	HRESULT DebugRedirect(IHttpContext* context, CNodeHttpStoredContext** ctx);
	HRESULT EnsureDebuggedApplicationKilled(IHttpContext* context, CNodeHttpStoredContext** ctx);	
	HRESULT GetOrCreateNodeApplication(IHttpContext* context, NodeDebugCommand debugCommand, CNodeApplication** application);
	HRESULT GetOrCreateNodeApplicationCore(PCWSTR physicalPath, DWORD physicalPathLength, IHttpContext* context, CNodeApplication** application);
	HRESULT GetOrCreateDebuggedNodeApplicationCore(PCWSTR physicalPath, DWORD physicalPathLength, NodeDebugCommand debugCommand, IHttpContext* context, CNodeApplication** application);
	CNodeApplication* TryGetExistingNodeApplication(PCWSTR physicalPath, DWORD physicalPathLength, BOOL debuggerRequest);
	HRESULT EnsureDebuggerFilesInstalled(PWSTR physicalPath, DWORD physicalPathSize);
	static BOOL CALLBACK EnsureDebuggerFile(HMODULE hModule, LPCTSTR lpszType, LPTSTR lpszName, LONG_PTR lParam);
	HRESULT RecycleApplicationCore(CNodeApplication* app);	
	HRESULT FindNextDebugPort(IHttpContext* context, DWORD* port);
	HRESULT EnsureDebugeeReady(IHttpContext* context, DWORD debugPort);

public:

	CNodeApplicationManager(IHttpServer* server, HTTP_MODULE_ID moduleId); 
	~CNodeApplicationManager();

	IHttpServer* GetHttpServer();
	HTTP_MODULE_ID GetModuleId();
	CAsyncManager* GetAsyncManager();
	CNodeEventProvider* GetEventProvider();
	CFileWatcher* GetFileWatcher();
	HANDLE GetJobObject();
	BOOL GetBreakAwayFromJobObject();

	static void OnScriptModified(CNodeApplicationManager* manager, CNodeApplication* application);

	HRESULT Initialize(IHttpContext* context);
	HRESULT Dispatch(IHttpContext* context, IHttpEventProvider* pProvider, CNodeHttpStoredContext** ctx);
	HRESULT RecycleApplication(CNodeApplication* app);
};

#endif