#ifndef __CNODEAPPLICATIONMANAGER_H__
#define __CNODEAPPLICATIONMANAGER_H__

class CNodeApplication;
class CAsyncManager;
class CFileWatcher;
class CNodeEventProvider;
class CNodeHttpStoredContext;
enum NodeDebugCommand;

#define MAX_BUFFER_SIZE 64

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
    SRWLOCK srwlock;
    CAsyncManager* asyncManager;
    HANDLE jobObject;
    BOOL breakAwayFromJobObject;
    CFileWatcher* fileWatcher;
    CNodeEventProvider* eventProvider;
    BOOL initialized;
    DWORD currentDebugPort;
    HMODULE inspector;
    LONG totalRequests;

    BOOL   _fSignalPipeInitialized;
    HANDLE signalPipe;
    HANDLE controlSignalHandlerThread;
    LPWSTR signalPipeName;
    PSECURITY_ATTRIBUTES pPipeSecAttr;
    WCHAR  debuggerExtensionDll[MAX_PATH];

    HANDLE GetSignalPipe();
    HRESULT SetupPipeSecurityAttributes();

    BOOL DirectoryExists(LPCWSTR directoryPath);
    HRESULT EnsureDirectoryStructureExists( LPCWSTR pszSkipPrefix, LPWSTR pszDirectoryPath );
    HRESULT DebugRedirect(IHttpContext* context, CNodeHttpStoredContext** ctx);
    HRESULT EnsureDebuggedApplicationKilled(IHttpContext* context, CNodeHttpStoredContext** ctx);	
    HRESULT GetOrCreateNodeApplication(IHttpContext* context, NodeDebugCommand debugCommand, BOOL allowCreate, CNodeApplication** application);
    HRESULT GetOrCreateNodeApplicationCore(PCWSTR physicalPath, DWORD physicalPathLength, IHttpContext* context, CNodeApplication** application);
    HRESULT GetOrCreateDebuggedNodeApplicationCore(PCWSTR physicalPath, DWORD physicalPathLength, NodeDebugCommand debugCommand, IHttpContext* context, CNodeApplication** application);
    CNodeApplication* TryGetExistingNodeApplication(PCWSTR physicalPath, DWORD physicalPathLength, BOOL debuggerRequest);
    HRESULT EnsureDebuggerFilesInstalled(PWSTR physicalPath, DWORD physicalPathSize, PWSTR debuggerExtensionDll);
    static BOOL CALLBACK EnsureDebuggerFile(HMODULE hModule, LPCTSTR lpszType, LPTSTR lpszName, LONG_PTR lParam);
    HRESULT RecycleApplicationCore(CNodeApplication* app);	
    HRESULT RecycleApplicationAssumeLock(CNodeApplication* app);
    HRESULT RecycleApplication(CNodeApplication* app, BOOL requiresLock);
    HRESULT FindNextDebugPort(IHttpContext* context, DWORD* port);
    HRESULT EnsureDebugeeReady(IHttpContext* context, DWORD debugPort);
    HRESULT InitializeCore(IHttpContext* context);
    HRESULT InitializeControlPipe();
    static unsigned int WINAPI ControlSignalHandler(void* arg);

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
    LPCWSTR GetSignalPipeName();
    PSECURITY_ATTRIBUTES GetPipeSecurityAttributes();
    HRESULT RecycleApplicationOnConfigChange(PCWSTR pszConfigPath);

    static void OnScriptModified(CNodeApplicationManager* manager, CNodeApplication* application);

    HRESULT Initialize(IHttpContext* context);
    HRESULT Dispatch(IHttpContext* context, IHttpEventProvider* pProvider, CNodeHttpStoredContext** ctx);
    HRESULT RecycleApplication(CNodeApplication* app);
    LONG GetTotalRequests();
};

#endif