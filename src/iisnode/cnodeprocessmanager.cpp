#include "precomp.h"

CNodeProcessManager::CNodeProcessManager(CNodeApplication* application, IHttpContext* context)
    : application(application), processes(NULL), currentProcess(0), isClosing(FALSE),
    refCount(1), gracefulShutdownProcessCount(0)
{
    if (this->GetApplication()->IsDebugMode())
    {
        this->processCount = 1;
    }
    else
    {
        this->processCount = CModuleConfiguration::GetNodeProcessCountPerApplication(context);
    }

    // cache event provider since the application can be disposed prior to CNodeProcessManager
    this->eventProvider = this->GetApplication()->GetApplicationManager()->GetEventProvider();

    this->gracefulShutdownTimeout = CModuleConfiguration::GetGracefulShutdownTimeout(context);
    InitializeSRWLock(&this->srwlock);
    this->gracefulShutdownDrainHandle = CreateEvent(NULL, TRUE, TRUE, NULL);
}

CNodeProcessManager::~CNodeProcessManager()
{
    this->Cleanup();
}

void CNodeProcessManager::Cleanup()
{
    if (NULL != this->processes)
    {
        for (int i = 0; i < this->processCount; i++)
        {
            if (this->processes[i])
            {
                delete this->processes[i];
            }
        }

        delete[] this->processes;
        this->processes = NULL;
    }

    CloseHandle(this->gracefulShutdownDrainHandle);
    this->gracefulShutdownDrainHandle = NULL;
}

CNodeApplication* CNodeProcessManager::GetApplication()
{
    return this->application;
}

HRESULT CNodeProcessManager::EmptyWorkingSet()
{
    ENTER_SRW_EXCLUSIVE(this->srwlock)

    for (int i = 0; i < this->processCount; i++)
    {
        if(this->processes[i] != NULL && !this->processes[i]->HasProcessExited())
        {
            SetProcessWorkingSetSize(this->processes[i]->GetProcess(), -1, -1);
        }
    }

    LEAVE_SRW_EXCLUSIVE(this->srwlock)

    return S_OK;
}

HRESULT CNodeProcessManager::Initialize(IHttpContext* context)
{
    HRESULT hr;

    ErrorIf(NULL == (this->processes = new CNodeProcess* [this->processCount]), ERROR_NOT_ENOUGH_MEMORY);
    RtlZeroMemory(this->processes, this->processCount * sizeof(CNodeProcess*));
    for (int i = 0; i < this->processCount; i++)
    {
        CheckError(this->AddProcess(i, context));
    }

    return S_OK;
Error:

    this->Cleanup();

    return hr;
}

HRESULT CNodeProcessManager::AddProcess(int ordinal, IHttpContext* context)
{
    HRESULT hr;

    ErrorIf(NULL != this->processes[ordinal], ERROR_INVALID_PARAMETER);
    ErrorIf(NULL == (this->processes[ordinal] = new CNodeProcess(this, context)), ERROR_NOT_ENOUGH_MEMORY);	
    CheckError(this->processes[ordinal]->Initialize(context));

    return S_OK;
Error:

    if (NULL != this->processes[ordinal])
    {
        delete this->processes[ordinal];
        this->processes[ordinal] = NULL;
    }

    return hr;
}

HRESULT CNodeProcessManager::Dispatch(CNodeHttpStoredContext* request)
{
    HRESULT hr;
    unsigned int tmpProcess, processToUse;

    CheckNull(request);

    this->AddRef(); // decremented at the bottom of this method

    if (!this->isClosing) 
    {
        ENTER_SRW_SHARED(this->srwlock)

        if (!this->isClosing)
        {
            // employ a round robin routing logic to get a "ticket" to use a process with a specific ordinal number
            
            if (1 == this->processCount)
            {
                processToUse = 0;
            }
            else
            {
                do 
                {
                    tmpProcess = this->currentProcess;
                    processToUse = (tmpProcess + 1) % this->processCount;
                } while (tmpProcess != InterlockedCompareExchange(&this->currentProcess, processToUse, tmpProcess));
            }

            // try dispatch to that process

            if (NULL != this->processes[processToUse]) 
            {
                CheckError(this->processes[processToUse]->AcceptRequest(request));
                request = NULL;
            }
        }

        LEAVE_SRW_SHARED(this->srwlock)

        if (NULL != request)
        {
            // the process to dispatch to does not exist and must be recreated

            ENTER_SRW_EXCLUSIVE(this->srwlock)

            if (!this->isClosing)
            {
                if (NULL == this->processes[processToUse])
                {
                    CheckError(this->AddProcess(processToUse, request->GetHttpContext()));
                }

                CheckError(this->processes[processToUse]->AcceptRequest(request));
                request = NULL;
            }

            LEAVE_SRW_EXCLUSIVE(this->srwlock)
        }
    }
    
    if (request)
    {
        this->GetEventProvider()->Log(request->GetHttpContext(), L"iisnode failed to accept a request beacuse the application is recycling", WINEVENT_LEVEL_ERROR, request->GetActivityId());
        CProtocolBridge::SendEmptyResponse(request, 503, CNodeConstants::IISNODE_ERROR_FAILED_ACCEPT_REQUEST_APP_RECYCLE, _T("Service Unavailable"), IISNODE_ERROR_APPLICATION_IS_RECYCLING);
    }

    this->DecRef(); // incremented at the beginning of this method

    return S_OK;
Error:

    this->GetEventProvider()->Log(request->GetHttpContext(),
        L"iisnode failed to initiate processing of a request", WINEVENT_LEVEL_ERROR);

    if (!CProtocolBridge::SendIisnodeError(request, hr))
    {
        CProtocolBridge::SendEmptyResponse(request, 503, CNodeConstants::IISNODE_ERROR_INIT_PROCESS_REQUEST, _T("Service Unavailable"), hr);
    }

    this->DecRef(); // incremented at the beginning of this method

    return hr;
}

HRESULT CNodeProcessManager::RecycleProcess(CNodeProcess* process)
{
    HRESULT hr;
    HANDLE recycler;
    ProcessRecycleArgs* args = NULL;
    BOOL gracefulRecycle = FALSE;
    CNodeApplication* appToRecycle = NULL;

    // remove the process from the process pool

    ENTER_SRW_EXCLUSIVE(this->srwlock)

    if (!this->isClosing)
    {
        appToRecycle = this->GetApplication();

        if (!appToRecycle->IsDebugMode())
        {
            appToRecycle = NULL;

            int i;
            for (i = 0; i < this->processCount; i++)
            {
                if (this->processes[i] == process)
                {
                    break;
                }
            }

            if (i == this->processCount)
            {
                // process not found in the active process list

                return S_OK;
            }

            this->processes[i] = NULL;

            // prevent the CNodeProcessManager from recycling until all CNodeProcesses that died finished graceful shutdown
            if (1L == InterlockedIncrement(&this->gracefulShutdownProcessCount))
            {
                ResetEvent(this->gracefulShutdownDrainHandle);
            }

            gracefulRecycle = TRUE;
        }
    }

    LEAVE_SRW_EXCLUSIVE(this->srwlock)

    // graceful recycle

    if (gracefulRecycle)
    {
        ErrorIf(NULL == (args = new ProcessRecycleArgs), ERROR_NOT_ENOUGH_MEMORY);
        args->count = 1;
        args->process = process;
        args->processes = &args->process;
        args->processManager = this;
        args->disposeApplication = FALSE;
        args->disposeProcess = TRUE;
        ErrorIf((HANDLE)-1L == (recycler = (HANDLE) _beginthreadex(NULL, 0, CNodeProcessManager::GracefulShutdown, args, 0, NULL)), ERROR_NOT_ENOUGH_MEMORY);
        CloseHandle(recycler);
    }

    if (appToRecycle)
    {
        appToRecycle->GetApplicationManager()->RecycleApplication(appToRecycle);
    }

    return S_OK;
Error:

    if (args)
    {
        delete args;
    }

    if (0L == InterlockedDecrement(&this->gracefulShutdownProcessCount))
    {
        SetEvent(this->gracefulShutdownDrainHandle);
    }

    return hr;
}

HRESULT CNodeProcessManager::Recycle()
{
    HRESULT hr;
    HANDLE recycler;
    ProcessRecycleArgs* args = NULL;

    ENTER_SRW_EXCLUSIVE(this->srwlock)

    this->isClosing = TRUE;	

    // perform actual recycling on a diffrent thread to free up the file watcher thread

    ErrorIf(NULL == (args = new ProcessRecycleArgs), ERROR_NOT_ENOUGH_MEMORY);
    args->count = this->processCount;
    args->process = NULL;
    args->processes = this->processes;
    args->processManager = this;
    args->disposeApplication = TRUE;
    args->disposeProcess = FALSE;
    ErrorIf((HANDLE)-1L == (recycler = (HANDLE) _beginthreadex(NULL, 0, CNodeProcessManager::GracefulShutdown, args, 0, NULL)), ERROR_NOT_ENOUGH_MEMORY);
    CloseHandle(recycler);

    LEAVE_SRW_EXCLUSIVE(this->srwlock)

    return S_OK;
Error:

    if (args)
    {
        delete args;
    }

    // if lack of memory is preventing us from initiating shutdown, we will just not provide auto update

    return hr;
}

unsigned int CNodeProcessManager::GracefulShutdown(void* arg)
{
    ProcessRecycleArgs* args = (ProcessRecycleArgs*)arg;
    HRESULT hr;
    HANDLE* drainHandles = NULL;
    DWORD drainHandleCount = 0;

    // drain active requests 

    ErrorIf(NULL == (drainHandles = new HANDLE[args->count + 1]), ERROR_NOT_ENOUGH_MEMORY);
    RtlZeroMemory(drainHandles, args->count * sizeof HANDLE);
    for (int i = 0; i < args->count; i++)
    {
        if (args->processes[i])
        {
            drainHandles[drainHandleCount] = CreateEvent(NULL, TRUE, FALSE, NULL);
            args->processes[i]->SignalWhenDrained(drainHandles[drainHandleCount]);
            drainHandleCount++;
        }
    }

    if (args->disposeApplication)
    {
        // prevent the application from exiting until pending graceful shutdown of died CNodeProcesses has finished
        drainHandles[drainHandleCount++] = args->processManager->gracefulShutdownDrainHandle;
    }
    
    if (args->processManager->gracefulShutdownTimeout > 0)
    {
        WaitForMultipleObjects(drainHandleCount, drainHandles, TRUE, args->processManager->gracefulShutdownTimeout);
    }

    if (args->disposeApplication)
    {
        // do not close the gracefulShutdownDrainHandle as it is owned by CNodeProcessManager
        drainHandleCount--;
    }

    for (int i = 0; i < drainHandleCount; i++)
    {
        CloseHandle(drainHandles[i]);
    }
    delete[] drainHandles;	
    drainHandles = NULL;

    if (args->disposeApplication)
    {
        // delete the application if requested (this will also delete process manager and all processes)
        // this is the application recycling code path

        delete args->processManager->GetApplication();
    }
    else if (args->disposeProcess)
    {
        // delete a single process if requested
        // this is the single process recycling code path (e.g. node.exe died)

        delete args->processes[0];
        if (0L == InterlockedDecrement(&args->processManager->gracefulShutdownProcessCount))
        {
            // release recycle of CNodeApplication that may be running concurrently
            SetEvent(args->processManager->gracefulShutdownDrainHandle);
        }
    }

    delete args;
    args = NULL;

    return S_OK;
Error:

    if (args)
    {
        if (args->disposeProcess)
        {
            if (0L == InterlockedDecrement(&args->processManager->gracefulShutdownProcessCount))
            {
                // release recycle of CNodeApplication that may be running concurrently
                SetEvent(args->processManager->gracefulShutdownDrainHandle);
            }
        }

        delete args;
        args = NULL;
    }

    if (drainHandles)
    {
        delete [] drainHandles;
        drainHandles = NULL;
    }

    return hr;
}

long CNodeProcessManager::AddRef()
{
    return InterlockedIncrement(&this->refCount);
}

long CNodeProcessManager::DecRef()
{
    long result = InterlockedDecrement(&this->refCount);

    if (0 == result)
    {
        delete this;
    }

    return result;
}

CNodeEventProvider* CNodeProcessManager::GetEventProvider()
{
    return this->eventProvider;
}

DWORD CNodeProcessManager::GetActiveRequestCount()
{
    DWORD result = 0;

    for (DWORD i = 0; i < this->processCount; i++)
    {
        if (this->processes[i])
        {
            result += this->processes[i]->GetActiveRequestCount();
        }
    }

    return result;
}

DWORD CNodeProcessManager::GetProcessCount()
{
    return this->processCount;
}