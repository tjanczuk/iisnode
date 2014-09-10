#ifndef __CNODEAPPLICATION_H__
#define __CNODEAPPLICATION_H__

class CNodeProcessManager;
class CFileWatcher;
class CNodeHttpStoredContext;
enum NodeDebugCommand;

class CNodeApplication
{
private:

    volatile static DWORD dwInternalAppId;
    PWSTR scriptName;
    PWSTR configPath;
    CNodeApplicationManager* applicationManager;
    CNodeProcessManager* processManager;
    CNodeApplication* peerApplication;
    BOOL isDebugger;
    NodeDebugCommand debugCommand;
    DWORD debugPort;
    BOOL needsRecycling;
    STTIMER *pIdleTimer;
    BOOL fIdleCallbackInProgress;
    BOOL fRequestsProcessedInLastIdleTimeoutPeriod;
    BOOL fEmptyW3WPWorkingSet; // flag to indicate that this application is responsible for emptying w3wp working set (only one app should be responsible).
    BOOL fEmptyWorkingSetAtStart;
    volatile DWORD dwIdlePageOutTimePeriod;

    void Cleanup();	

public:

    CNodeApplication(CNodeApplicationManager* applicationManager, BOOL isDebugger, NodeDebugCommand debugCommand, DWORD debugPort);	
    ~CNodeApplication();

    static
    VOID
    CALLBACK
    IdleTimerCallback(
        IN PTP_CALLBACK_INSTANCE Instance,
        IN PVOID Context,
        IN PTP_TIMER Timer
    );

    HRESULT EmptyWorkingSet();
    HRESULT Initialize(PCWSTR scriptName, IHttpContext* context);
    PCWSTR GetScriptName();
    PCWSTR GetConfigPath();
    HRESULT SetConfigPath(IHttpContext* context);
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
    void SetNeedsRecycling();
    BOOL GetNeedsRecycling();
};

#endif