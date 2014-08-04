#include "precomp.h"
#include <sddl.h>

CNodeApplicationManager::CNodeApplicationManager(IHttpServer* server, HTTP_MODULE_ID moduleId)
    : server(server), moduleId(moduleId), applications(NULL), asyncManager(NULL), jobObject(NULL), 
    breakAwayFromJobObject(FALSE), fileWatcher(NULL), initialized(FALSE), eventProvider(NULL),
    currentDebugPort(0), inspector(NULL), totalRequests(0), controlSignalHandlerThread(NULL), 
    signalPipe(NULL), signalPipeName(NULL), pPipeSecAttr(NULL), _fSignalPipeInitialized( FALSE )
{
    InitializeSRWLock(&this->srwlock);
}

HRESULT CNodeApplicationManager::Initialize(IHttpContext* context)
{
    HRESULT hr = S_OK;
    CModuleConfiguration *config;

    if (S_OK != (hr = CModuleConfiguration::GetConfig(context, &config)))
    {
        hr = IISNODE_ERROR_UNABLE_TO_READ_CONFIGURATION_OVERRIDE == hr ? hr : IISNODE_ERROR_UNABLE_TO_READ_CONFIGURATION;
    }
    else if (!this->initialized)
    {
        ENTER_SRW_EXCLUSIVE(this->srwlock)

        if (!this->initialized)
        {
            hr = this->InitializeCore(context);
        }

        LEAVE_SRW_EXCLUSIVE(this->srwlock)
    }

    return hr;
}

LONG CNodeApplicationManager::GetTotalRequests()
{
    return this->totalRequests;
}

LPCWSTR CNodeApplicationManager::GetSignalPipeName()
{
    return this->signalPipeName;
}

HANDLE CNodeApplicationManager::GetSignalPipe()
{
    return this->signalPipe;
}

unsigned int CNodeApplicationManager::ControlSignalHandler(void* arg)
{
    CNodeApplicationManager*    nodeApplicationManager = (CNodeApplicationManager*)arg;
    BYTE                        buffer[MAX_BUFFER_SIZE] = {0};
    DWORD                       bytesRead = 0;

    _ASSERT(nodeApplicationManager != NULL);
    
    //
    // wait for a client to connect -- does not support multiple clients yet.
    //
    if(ConnectNamedPipe(nodeApplicationManager->GetSignalPipe(), NULL) == 0)
    {
        // failed
        nodeApplicationManager->GetEventProvider()->Log(L"iisnode control signal pipe client connection failed", WINEVENT_LEVEL_ERROR);
        goto Finished;
    }

    //
    // Read message from pipe.
    //
    if(ReadFile(nodeApplicationManager->GetSignalPipe(), buffer, MAX_BUFFER_SIZE, &bytesRead, NULL))
    {
        if(strnicmp((char*)buffer, "recycle", MAX_BUFFER_SIZE) == 0)
        {
            nodeApplicationManager->GetHttpServer()->RecycleProcess(L"Node.exe signalled recycle");
        }
    }

Finished:
    return 0;
}

PSECURITY_ATTRIBUTES 
CNodeApplicationManager::GetPipeSecurityAttributes(
    VOID
)
{
    return this->pPipeSecAttr;
}

HRESULT CNodeApplicationManager::InitializeControlPipe()
{
    HRESULT hr = S_OK;
    UUID uuid;
    RPC_WSTR suuid = NULL;

    if( _fSignalPipeInitialized )
    {
        return hr;
    }

    //
    // generate unique pipe name
    //

    ErrorIf(RPC_S_OK != UuidCreate(&uuid), ERROR_CAN_NOT_COMPLETE);
    ErrorIf(RPC_S_OK != UuidToStringW(&uuid, &suuid), ERROR_NOT_ENOUGH_MEMORY);
    ErrorIf((this->signalPipeName = new WCHAR[1024]) == NULL, ERROR_NOT_ENOUGH_MEMORY);
    wcscpy(this->signalPipeName, L"\\\\.\\pipe\\");
    wcscpy(this->signalPipeName + 9, (WCHAR*)suuid);
    RpcStringFreeW(&suuid);
    suuid = NULL;

    this->signalPipe = CreateNamedPipeW( this->signalPipeName,
                                       PIPE_ACCESS_INBOUND,
                                       PIPE_TYPE_MESSAGE | PIPE_WAIT,
                                       1,
                                       MAX_BUFFER_SIZE,
                                       MAX_BUFFER_SIZE,
                                       0,
                                       GetPipeSecurityAttributes() );

    ErrorIf( this->signalPipe == INVALID_HANDLE_VALUE, 
             HRESULT_FROM_WIN32(ERROR_INVALID_HANDLE) );

    //
    // start pipe reader thread.
    //

    this->controlSignalHandlerThread = (HANDLE) _beginthreadex( NULL, 
                                                          0, 
                                                          CNodeApplicationManager::ControlSignalHandler, 
                                                          this, 
                                                          0, 
                                                          NULL);

    ErrorIf((HANDLE)-1L == this->controlSignalHandlerThread, ERROR_NOT_ENOUGH_MEMORY);

    this->GetEventProvider()->Log(L"iisnode initialized control pipe", WINEVENT_LEVEL_INFO);

    _fSignalPipeInitialized = TRUE;

Error:

    if(suuid != NULL)
    {
        RpcStringFreeW(&suuid);
        suuid = NULL;
    }

    if( FAILED( hr ) )
    {
        if(NULL != this->controlSignalHandlerThread)
        {
            CloseHandle(this->controlSignalHandlerThread);
            this->controlSignalHandlerThread = NULL;
        }

        if(NULL != this->signalPipe)
        {
            CloseHandle(this->signalPipe);
            this->signalPipe = NULL;
        }

        if(NULL != this->signalPipeName)
        {
            delete[] this->signalPipeName;
            this->signalPipeName = NULL;
        }
    }

    return hr;
}

HRESULT CNodeApplicationManager::InitializeCore(IHttpContext* context)
{
    HRESULT hr;
    BOOL isInJob, createJob;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobInfo;

    ErrorIf(NULL == (this->eventProvider = new CNodeEventProvider()), ERROR_NOT_ENOUGH_MEMORY);
    CheckError(this->eventProvider->Initialize());
    ErrorIf(NULL != this->asyncManager, ERROR_INVALID_OPERATION);
    ErrorIf(NULL == (this->asyncManager = new CAsyncManager()), ERROR_NOT_ENOUGH_MEMORY);
    CheckError(this->asyncManager->Initialize(context));
    ErrorIf(NULL == (this->fileWatcher = new CFileWatcher()), ERROR_NOT_ENOUGH_MEMORY);
    CheckError(this->fileWatcher->Initialize(context));

    CheckError(CUtils::CreatePipeSecurity(&this->pPipeSecAttr));

    // determine whether node processes should be created in a new job object
    // or whether current job object is adequate; the goal is to kill node processes when
    // the IIS worker process is killed while preserving current job limits, if any
    
    ErrorIf(!IsProcessInJob(GetCurrentProcess(), NULL, &isInJob), HRESULT_FROM_WIN32(GetLastError()));
    if (!isInJob)
    {
        createJob = TRUE;
    }
    else
    {
        ErrorIf(!QueryInformationJobObject(NULL, JobObjectExtendedLimitInformation, &jobInfo, sizeof jobInfo, NULL), 
            HRESULT_FROM_WIN32(GetLastError()));

        if (jobInfo.BasicLimitInformation.LimitFlags & JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK )
        {
            createJob = TRUE;
        }
        else if(jobInfo.BasicLimitInformation.LimitFlags & JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE )
        {
            createJob = FALSE;
        }
        else if(jobInfo.BasicLimitInformation.LimitFlags & JOB_OBJECT_LIMIT_BREAKAWAY_OK )
        {
            createJob = TRUE;
            this->breakAwayFromJobObject = TRUE;
        }
        else
        {
            createJob = TRUE;
        }
    }

    if (createJob)
    {
        ErrorIf(NULL == (this->jobObject = CreateJobObject(NULL, NULL)), HRESULT_FROM_WIN32(GetLastError()));
        RtlZeroMemory(&jobInfo, sizeof jobInfo);
        jobInfo.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        ErrorIf(!SetInformationJobObject(this->jobObject, JobObjectExtendedLimitInformation, &jobInfo, sizeof jobInfo), 
            HRESULT_FROM_WIN32(GetLastError()));
    }

    if(CModuleConfiguration::GetRecycleSignalEnabled(context) && !_fSignalPipeInitialized)
    {
        CheckError(InitializeControlPipe());
    }

    this->initialized = TRUE;

    this->GetEventProvider()->Log(context, L"iisnode initialized the application manager", WINEVENT_LEVEL_INFO);

    return S_OK;
Error:

    this->GetEventProvider()->Log(context, L"iisnode failed to initialize the application manager", WINEVENT_LEVEL_ERROR);

    if (NULL != this->asyncManager)
    {
        delete this->asyncManager;
        this->asyncManager = NULL;
    }

    if (NULL != this->jobObject)
    {
        CloseHandle(this->jobObject);
        this->jobObject = NULL;
    }

    if (NULL != this->fileWatcher)
    {
        delete this->fileWatcher;
        this->fileWatcher = NULL;
    }

    return hr;
}

CNodeApplicationManager::~CNodeApplicationManager()
{
    HANDLE  hPipe = NULL;

    while (NULL != this->applications)
    {
        delete this->applications->nodeApplication;
        NodeApplicationEntry* current = this->applications;
        this->applications = this->applications->next;
        delete current;
    }

    if( _fSignalPipeInitialized )
    {
        //
        // try to connect to the pipe to unblock the thread
        // waiting on ConnectNamedPipe
        // We dont need to check any errors on whether the connect
        // succeeded because we dont care.
        //

        hPipe = CreateFileW( this->GetSignalPipeName(), 
                             GENERIC_WRITE,
                             0,
                             NULL,
                             OPEN_EXISTING,
                             0,
                             NULL );

        if(hPipe != NULL)
        {
            CloseHandle(hPipe);
            hPipe = NULL;
        }
    }

    if(NULL != this->controlSignalHandlerThread)
    {
        CloseHandle(this->controlSignalHandlerThread);
        this->controlSignalHandlerThread = NULL;
    }

    if(NULL != this->signalPipe)
    {
        CloseHandle(this->signalPipe);
        this->signalPipe = NULL;
    }

    if(NULL != this->pPipeSecAttr)
    {
        CUtils::FreePipeSecurity(this->pPipeSecAttr);
        this->pPipeSecAttr = NULL;
    }

    if(NULL != this->signalPipeName)
    {
        delete[] this->signalPipeName;
        this->signalPipeName = NULL;
    }

    if (NULL != this->asyncManager)
    {
        delete this->asyncManager;
        this->asyncManager = NULL;
    }

    if (NULL != this->jobObject)
    {
        CloseHandle(this->jobObject);
        this->jobObject = NULL;
    }

    if (NULL != this->fileWatcher)
    {
        delete this->fileWatcher;
        this->fileWatcher = NULL;
    }

    if (NULL != this->eventProvider)
    {
        delete this->eventProvider;
        this->eventProvider = NULL;
    }
}

IHttpServer* CNodeApplicationManager::GetHttpServer()
{
    return this->server;
}

HTTP_MODULE_ID CNodeApplicationManager::GetModuleId()
{
    return this->moduleId;
}

CAsyncManager* CNodeApplicationManager::GetAsyncManager()
{
    return this->asyncManager;
}

HRESULT CNodeApplicationManager::Dispatch(IHttpContext* context, IHttpEventProvider* pProvider, CNodeHttpStoredContext** ctx)
{
    HRESULT hr = S_OK;
    CNodeApplication* application;
    NodeDebugCommand debugCommand;

    CheckNull(ctx);
    *ctx = NULL;

    InterlockedIncrement(&this->totalRequests);

    CheckError(CNodeDebugger::GetDebugCommand(context, this->GetEventProvider(), &debugCommand));

    switch (debugCommand) 
    {
    default:

        if(CModuleConfiguration::GetRecycleSignalEnabled(context) && !_fSignalPipeInitialized)
        {
            ENTER_SRW_EXCLUSIVE(this->srwlock)

            if(CModuleConfiguration::GetRecycleSignalEnabled(context) && !_fSignalPipeInitialized)
            {
                CheckError(InitializeControlPipe());
            }

            LEAVE_SRW_EXCLUSIVE(this->srwlock)
        }

        ENTER_SRW_SHARED(this->srwlock)

        CheckError(this->GetOrCreateNodeApplication(context, debugCommand, FALSE, &application));
        if (application && !application->GetNeedsRecycling())
        {
            // this is the sweetspot code path: application already exists, shared read lock is sufficient

            CheckError(application->Dispatch(context, pProvider, ctx));
        }

        LEAVE_SRW_SHARED(this->srwlock)

        if (!application || application->GetNeedsRecycling())
        {
            // this is the initialization code path for activating request:
            // application must be created which requires an exclusive lock

            ENTER_SRW_EXCLUSIVE(this->srwlock)

            CheckError(this->GetOrCreateNodeApplication(context, debugCommand, TRUE, &application));
            if (application->GetNeedsRecycling())
            {
                this->RecycleApplicationAssumeLock(application);
                CheckError(this->GetOrCreateNodeApplication(context, debugCommand, TRUE, &application));
            }

            CheckError(application->Dispatch(context, pProvider, ctx));

            LEAVE_SRW_EXCLUSIVE(this->srwlock)
        }

        break;

    case ND_KILL:

        ENTER_SRW_EXCLUSIVE(this->srwlock)

        CheckError(this->EnsureDebuggedApplicationKilled(context, ctx));

        LEAVE_SRW_EXCLUSIVE(this->srwlock)

        break;

    case ND_REDIRECT:

        // redirection from e.g. app.js/debug to app.js/debug/

        CheckError(this->DebugRedirect(context, ctx));

        break;
    };

    return S_OK;
Error:
    return hr;
}

HRESULT CNodeApplicationManager::DebugRedirect(IHttpContext* context, CNodeHttpStoredContext** ctx)
{
    HRESULT hr;

    ErrorIf(NULL == (*ctx = new CNodeHttpStoredContext(NULL, this->GetEventProvider(), context)), ERROR_NOT_ENOUGH_MEMORY);
    IHttpModuleContextContainer* moduleContextContainer = context->GetModuleContextContainer();
    moduleContextContainer->SetModuleContext(*ctx, this->GetModuleId());
    (*ctx)->IncreasePendingAsyncOperationCount(); // will be decreased in CProtocolBridge::FinalizeResponseCore
    CheckError(CProtocolBridge::SendDebugRedirect(*ctx, this->GetEventProvider()));

    return S_OK;
Error:
    return hr;
}

HRESULT CNodeApplicationManager::EnsureDebuggedApplicationKilled(IHttpContext* context, CNodeHttpStoredContext** ctx)
{
    HRESULT hr;
    DWORD physicalPathLength;
    PCWSTR physicalPath = context->GetScriptTranslated(&physicalPathLength);
    DWORD killCount = 0;

    CNodeApplication* app = this->TryGetExistingNodeApplication(physicalPath, physicalPathLength, TRUE);
    if (app) killCount++;
    CheckError(this->RecycleApplication(app, FALSE));
    app = this->TryGetExistingNodeApplication(physicalPath, physicalPathLength, FALSE);
    if (app) killCount++;
    CheckError(this->RecycleApplication(app, FALSE));

    if (ctx)
    {
        ErrorIf(NULL == (*ctx = new CNodeHttpStoredContext(NULL, this->GetEventProvider(), context)), ERROR_NOT_ENOUGH_MEMORY);
        IHttpModuleContextContainer* moduleContextContainer = context->GetModuleContextContainer();
        moduleContextContainer->SetModuleContext(*ctx, this->GetModuleId());        
        (*ctx)->SetRequestNotificationStatus(RQ_NOTIFICATION_CONTINUE);
        if (killCount > 0)
        {
            CProtocolBridge::SendSyncResponse(context, 200, "OK", S_OK, TRUE, "The debugger and debugee processes have been killed.");
        }
        else
        {
            CProtocolBridge::SendSyncResponse(context, 200, "OK", S_OK, TRUE, "The debugger for the application is not running.");
        }
    }

    return S_OK;
Error:
    return hr;
}

HRESULT CNodeApplicationManager::RecycleApplicationOnConfigChange(PCWSTR pszConfigPath)
{
    HRESULT hr = ERROR_NOT_FOUND;

    ENTER_SRW_EXCLUSIVE(this->srwlock)

    NodeApplicationEntry* current = this->applications;
    while (current)
    {
        //
        // recycle apps if any config along its configPath hierarchy changes.
        //

        if (wcsstr(current->nodeApplication->GetConfigPath(), pszConfigPath) != NULL)
        {
            current->nodeApplication->SetNeedsRecycling();
            hr = S_OK;
        }

        current = current->next;
    }

    LEAVE_SRW_EXCLUSIVE(this->srwlock)

    return hr;
}

HRESULT CNodeApplicationManager::RecycleApplication(CNodeApplication* app)
{
    return this->RecycleApplication(app, TRUE);
}

HRESULT CNodeApplicationManager::RecycleApplication(CNodeApplication* app, BOOL requiresLock)
{
    HRESULT hr;

    if (requiresLock)
    {
        ENTER_SRW_EXCLUSIVE(this->srwlock)

        hr = this->RecycleApplicationAssumeLock(app);

        LEAVE_SRW_EXCLUSIVE(this->srwlock)
    }
    else
    {
        hr = this->RecycleApplicationAssumeLock(app);
    }

    return hr;
}

// this method is always called under exclusive this->srwlock
HRESULT CNodeApplicationManager::RecycleApplicationAssumeLock(CNodeApplication* app)
{
    // ensure the application still exists to avoid race condition with other recycling code paths

    NodeApplicationEntry* current = this->applications;
    while (current)
    {
        if (current->nodeApplication == app)
        {
            this->RecycleApplicationCore(app->GetPeerApplication());
            this->RecycleApplicationCore(app);
            break;
        }

        current = current->next;
    }

    return S_OK;
}

void CNodeApplicationManager::OnScriptModified(CNodeApplicationManager* manager, CNodeApplication* application)
{
    application->SetNeedsRecycling();
}

// this method is always called under exclusive this->srwlock
HRESULT CNodeApplicationManager::RecycleApplicationCore(CNodeApplication* app)
{
    HRESULT hr;

    if (app)
    {
        NodeApplicationEntry* previous = NULL;
        NodeApplicationEntry* applicationEntry = this->applications;
        while (applicationEntry && applicationEntry->nodeApplication != app) 
        {
            previous = applicationEntry;
            applicationEntry = applicationEntry->next;
        }

        if (applicationEntry && applicationEntry->nodeApplication == app)
        {
            CheckError(applicationEntry->nodeApplication->Recycle());
            if (previous)
                previous->next = applicationEntry->next;
            else
                this->applications = applicationEntry->next;
            delete applicationEntry;
        }

        // beyond this point, the the application manager will not be dispatching new messages to the application
    }

    return S_OK;
Error:
    return hr;
}

HRESULT CNodeApplicationManager::GetOrCreateNodeApplicationCore(PCWSTR physicalPath, DWORD physicalPathLength, IHttpContext* context, CNodeApplication** application)
{
    HRESULT hr;    
    NodeApplicationEntry* applicationEntry = NULL;

    if (NULL == (*application = this->TryGetExistingNodeApplication(physicalPath, physicalPathLength, FALSE)))
    {
        ErrorIf(NULL == (applicationEntry = new NodeApplicationEntry()), ERROR_NOT_ENOUGH_MEMORY);
        ErrorIf(NULL == (applicationEntry->nodeApplication = new CNodeApplication(this, FALSE, ND_NONE, 0)), ERROR_NOT_ENOUGH_MEMORY);
        CheckError(applicationEntry->nodeApplication->Initialize(physicalPath, context));

        *application = applicationEntry->nodeApplication;
        applicationEntry->next = this->applications;
        this->applications = applicationEntry;
    }    
    else
    {
        this->GetEventProvider()->Log(context, L"iisnode found an existing node.js application to dispatch the http request to", WINEVENT_LEVEL_VERBOSE);
    }

    return S_OK;
Error:

    if (NULL != applicationEntry)
    {
        if (NULL != applicationEntry->nodeApplication)
        {
            delete applicationEntry->nodeApplication;
        }
        delete applicationEntry;
    }
    
    this->GetEventProvider()->Log(context, L"iisnode failed to create a new node.js application", WINEVENT_LEVEL_ERROR);

    return hr;
}

HRESULT CNodeApplicationManager::EnsureDebuggerFilesInstalled(PWSTR physicalPath, DWORD physicalPathSize, LPWSTR debuggerExtensionDll)
{
    HRESULT hr;
    HMODULE iisnode;
    DebuggerFileEnumeratorParams params = { S_OK, physicalPath, wcslen(physicalPath), physicalPathSize };
    BOOL fLoadModule = FALSE;

    // Process all resources of DEBUGGERFILE type (256) defined in resource.rc and save to disk if necessary
    ErrorIf(debuggerExtensionDll == NULL, ERROR_INVALID_PARAMETER);
    ErrorIf(wcslen(debuggerExtensionDll) >= MAX_PATH, ERROR_INVALID_PARAMETER);

    if(wcsicmp(this->debuggerExtensionDll, debuggerExtensionDll) != 0)
    {
        fLoadModule = TRUE;
    }

    if (NULL == this->inspector || fLoadModule)
    {
        // try loading iisnode-inspector.dll from the same location where iisnode.dll is located

        if(this->inspector != NULL)
        {
            FreeLibrary(this->inspector);
            this->inspector = NULL;
        }

        WCHAR path[MAX_PATH];
        DWORD size;
        ErrorIf(NULL == (iisnode = GetModuleHandleW(L"iisnode.dll")), GetLastError());
        ErrorIf(0 == (size = GetModuleFileNameW(iisnode, path, MAX_PATH)), GetLastError());
        ErrorIf(size == MAX_PATH || S_OK != GetLastError(), E_FAIL);

        //
        // path points to d:\program files\iisnode\iisnode.dll for example.
        // need to replace iisnode.dll with the debuggerExtensionDll which is why
        // path + size - 11 (11 is length of iisnode.dll)
        //

        ErrorIf((size + wcslen(debuggerExtensionDll) - 11) >= MAX_PATH, E_FAIL);
        wcscpy(path + size - 11, debuggerExtensionDll);
        ErrorIf(NULL == (this->inspector = LoadLibraryExW(path, NULL, LOAD_LIBRARY_AS_DATAFILE)), IISNODE_ERROR_INSPECTOR_NOT_FOUND);

        wcscpy(this->debuggerExtensionDll, debuggerExtensionDll);
    }

    ErrorIf(!EnumResourceNames(this->inspector, "DEBUGGERFILE", CNodeApplicationManager::EnsureDebuggerFile, (LONG_PTR)&params), GetLastError());
    CheckError(params.hr);

    this->GetEventProvider()->Log(
        L"iisnode unpacked debugger files", WINEVENT_LEVEL_INFO);

    return S_OK;
Error:

    if(this->inspector != NULL)
    {
        FreeLibrary(this->inspector);
        this->inspector = NULL;
    }

    this->GetEventProvider()->Log(
        L"iisnode failed to unpack debugger files", WINEVENT_LEVEL_ERROR);

    return IISNODE_ERROR_UNABLE_TO_CREATE_DEBUGGER_FILES;
}

BOOL CALLBACK CNodeApplicationManager::EnsureDebuggerFile(HMODULE hModule, LPCTSTR lpszType, LPTSTR lpszName, LONG_PTR lParam)
{
    BOOL result = TRUE;
    HRESULT hr;
    DebuggerFileEnumeratorParams* params = (DebuggerFileEnumeratorParams*)lParam;
    HANDLE file = INVALID_HANDLE_VALUE;
    HRSRC resource1;
    HGLOBAL resource2;
    void* resource3;
    DWORD sizeOfFile;
    DWORD bytesWritten;
    size_t converted;
    DWORD lastDirectoryEnd, currentDirectoryEnd;
    DWORD dwResourceIndex = 0;
    CHAR  buffer[MAX_PATH];

    ErrorIf(NULL == lpszName, ERROR_INVALID_PARAMETER);

    //
    // resource file ID cannot start with a number so the format of the ID is F<Index>.
    // lpszName is of the format F<Index>, so just skip the first character and convert the <Index> to number
    //
    
    ErrorIf(strlen(lpszName) <= 1, ERROR_INVALID_PARAMETER);
    ErrorIf(*lpszName != 'F', ERROR_INVALID_PARAMETER);

    dwResourceIndex = atoi(lpszName + 1);

    ErrorIf(LoadStringA(hModule, dwResourceIndex, buffer, MAX_PATH) == 0, GetLastError());

    // append file name to the base directory name

    CheckError(mbstowcs_s(
        &converted, 
        params->physicalFile + params->physicalFileLength, 
        params->physicalFileSize - params->physicalFileLength - 1, 
        buffer, 
        _TRUNCATE));

    // ensure the directory structure is created

    for (lastDirectoryEnd = wcslen(params->physicalFile); 
        params->physicalFile[lastDirectoryEnd] != L'\\'; 
        lastDirectoryEnd--); // the \ character is guaranteed to exist in physicalPath since it has been added in GetOrCreateDebuggedNodeApplicationCore

    currentDirectoryEnd = params->physicalFileLength - 1;
    do {
        params->physicalFile[currentDirectoryEnd] = L'\0';

        if (!CreateDirectoryW(params->physicalFile, NULL))
        {
            hr = GetLastError();
            if (ERROR_ALREADY_EXISTS != hr)
            {
                CheckError(hr);
            }
        }
        params->physicalFile[currentDirectoryEnd++] = L'\\';
        while (currentDirectoryEnd <= lastDirectoryEnd && params->physicalFile[currentDirectoryEnd] != L'\\')
            currentDirectoryEnd++;
    } 
    while (currentDirectoryEnd <= lastDirectoryEnd);

    // check if the file already exists and create it if it does not, if its present, overwrite it.
    file = CreateFileW(params->physicalFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    
    if (INVALID_HANDLE_VALUE == file)
    {
        ErrorIf(ERROR_FILE_EXISTS != (hr = GetLastError()), hr);
    }
    else
    {
        ErrorIf(NULL == (resource1 = FindResource(hModule, lpszName, lpszType)), GetLastError());
        ErrorIf(NULL == (resource2 = LoadResource(hModule, resource1)), GetLastError());
        ErrorIf(NULL == (resource3 = LockResource(resource2)), ERROR_INVALID_PARAMETER);
        ErrorIf(0 == (sizeOfFile = SizeofResource(hModule, resource1)), GetLastError());
        ErrorIf(!WriteFile(file, resource3, sizeOfFile, &bytesWritten, NULL), GetLastError());
        CloseHandle(file);
        file = INVALID_HANDLE_VALUE;
    }

    params->physicalFile[params->physicalFileLength] = 0; // truncate the path for processing of the next file

    return result;
Error:

    params->hr = hr;
    params->physicalFile[params->physicalFileLength] = 0; // truncate the path for processing of the next file

    if (INVALID_HANDLE_VALUE != file)
    {
        CloseHandle(file);
        file = INVALID_HANDLE_VALUE;
    }

    return FALSE;
}

HRESULT CNodeApplicationManager::GetOrCreateDebuggedNodeApplicationCore(PCWSTR physicalPath, DWORD physicalPathLength, NodeDebugCommand debugCommand, IHttpContext* context, CNodeApplication** application)
{
    HRESULT hr;    
    NodeApplicationEntry* applicationEntry = NULL;
    NodeApplicationEntry* debuggerEntry = NULL;
    PWSTR debuggerPath;
    DWORD debugPort;

    // This method encures there is a CNodeApplication representing the debugger application 
    // and that it is paired with a debugged node.js application.
    // If an instance of an application to be debugged exists but it is not running in debug mode,
    // it will be purged and created anew in debug mode.

    if (NULL == (*application = this->TryGetExistingNodeApplication(physicalPath, physicalPathLength, TRUE)))
    {
        // debugger for the application does not exist yet

        // ensure debugger application files are available on disk

        LPWSTR debuggerVirtualDir = CModuleConfiguration::GetDebuggerVirtualDir(context);
        ErrorIf(debuggerVirtualDir == NULL, ERROR_INVALID_DATA);
        DWORD debuggerVirtualDirLength = CModuleConfiguration::GetDebuggerVirtualDirLength(context);

        DWORD debuggerPathSize = 0;

        if(debuggerVirtualDirLength == 0)
        {
            // default location for debugger related files is under site root dir.
            debuggerPathSize = wcslen(physicalPath) + CModuleConfiguration::GetDebuggerPathSegmentLength(context) + 512;
            ErrorIf(NULL == (debuggerPath = (PWSTR)context->AllocateRequestMemory(sizeof WCHAR * debuggerPathSize)), ERROR_NOT_ENOUGH_MEMORY);
            swprintf(debuggerPath, L"%s.%s\\", physicalPath, CModuleConfiguration::GetDebuggerPathSegment(context));
        }
        else
        {
            // create debugger files in the virtual directory's physical path.

            LPWSTR debuggerVirtualDirPhysicalPath = CModuleConfiguration::GetDebuggerVirtualDirPhysicalPath(context);
            ErrorIf(debuggerVirtualDirPhysicalPath == NULL, HRESULT_FROM_WIN32(ERROR_DIRECTORY));
            
            DWORD debuggerVirtualDirPhysicalPathLength = wcslen(debuggerVirtualDirPhysicalPath);
            ErrorIf(debuggerVirtualDirPhysicalPathLength == 0, HRESULT_FROM_WIN32(ERROR_DIRECTORY));

            debuggerPathSize = debuggerVirtualDirPhysicalPathLength + wcslen(physicalPath) + CModuleConfiguration::GetDebuggerPathSegmentLength(context) + 512;
            ErrorIf(NULL == (debuggerPath = (PWSTR)context->AllocateRequestMemory(sizeof WCHAR * debuggerPathSize)), ERROR_NOT_ENOUGH_MEMORY);

            LPWSTR debuggerFilesPathSegment = CModuleConfiguration::GetDebuggerFilesPathSegment(context);
            DWORD debuggerFilesPathSegmentLen = CModuleConfiguration::GetDebuggerFilesPathSegmentLength(context);

            PCWSTR appName = physicalPath + physicalPathLength - 1;
            while (appName > physicalPath && *appName != L'\\')
                appName--;
            appName++;

            if( debuggerVirtualDirPhysicalPath[ debuggerVirtualDirPhysicalPathLength - 1 ] == L'\\')
            {                            
                swprintf(debuggerPath, 
                     L"%s%s\\%s.%s\\",
                     debuggerVirtualDirPhysicalPath,
                     debuggerFilesPathSegment,
                     appName,
                     CModuleConfiguration::GetDebuggerPathSegment(context));
            }
            else
            {
                swprintf(debuggerPath, 
                     L"%s\\%s\\%s.%s\\",
                     debuggerVirtualDirPhysicalPath,
                     debuggerFilesPathSegment,
                     appName,
                     CModuleConfiguration::GetDebuggerPathSegment(context));
            }

            CheckError(EnsureDirectoryStructureExists(debuggerVirtualDirPhysicalPath, debuggerPath));

            // here debuggerPath would look like "VDirPhysicalPath\sha256Hash(scriptPath)\script.js.debug\"
            // which is where EnsureDebuggerFilesInstalled will install all debugger files.
        }

        CheckError(this->EnsureDebuggerFilesInstalled(debuggerPath, debuggerPathSize, CModuleConfiguration::GetDebuggerExtensionDll(context)));

        // kill any existing instance of the application to debug and the debugger

        CheckError(this->EnsureDebuggedApplicationKilled(context, NULL));

        // determine next available debugging port

        CheckError(this->FindNextDebugPort(context, &debugPort));

        // create debuggee

        ErrorIf(NULL == (applicationEntry = new NodeApplicationEntry()), ERROR_NOT_ENOUGH_MEMORY);
        ErrorIf(NULL == (applicationEntry->nodeApplication = new CNodeApplication(this, FALSE, debugCommand, debugPort)), ERROR_NOT_ENOUGH_MEMORY);

        // create debugger

        ErrorIf(NULL == (debuggerEntry = new NodeApplicationEntry()), ERROR_NOT_ENOUGH_MEMORY);
        ErrorIf(NULL == (debuggerEntry->nodeApplication = new CNodeApplication(this, TRUE, debugCommand, debugPort)), ERROR_NOT_ENOUGH_MEMORY);

        // associate debugger with debuggee

        debuggerEntry->nodeApplication->SetPeerApplication(applicationEntry->nodeApplication);
        applicationEntry->nodeApplication->SetPeerApplication(debuggerEntry->nodeApplication);

        // initialize debugee

        CheckError(applicationEntry->nodeApplication->Initialize(physicalPath, context));
        CheckError(this->EnsureDebugeeReady(context, debugPort));

        // initialize debugger

        wcscat(debuggerPath, L"node_modules\\node-inspector\\bin\\inspector.js");
        CheckError(debuggerEntry->nodeApplication->Initialize(debuggerPath, context));

        // add debugger and debuggee to the list of applications and return the debugger application

        *application = debuggerEntry->nodeApplication;
        applicationEntry->next = this->applications;
        debuggerEntry->next = applicationEntry;
        this->applications = debuggerEntry;
    }    
    else
    {
        this->GetEventProvider()->Log(context, L"iisnode found an existing node.js debugger to dispatch the http request to", WINEVENT_LEVEL_VERBOSE);
    }

    return S_OK;
Error:

    if (NULL != applicationEntry)
    {
        if (NULL != applicationEntry->nodeApplication)
        {
            delete applicationEntry->nodeApplication;
        }
        delete applicationEntry;
    }

    if (NULL != debuggerEntry)
    {
        if (NULL != debuggerEntry->nodeApplication)
        {
            delete debuggerEntry->nodeApplication;
        }
        delete debuggerEntry;
    }

    this->GetEventProvider()->Log(context, L"iisnode failed to create a new node.js application to debug or the debugger for that application", WINEVENT_LEVEL_ERROR);

    return hr;
}

HRESULT CNodeApplicationManager::EnsureDirectoryStructureExists( LPCWSTR pszSkipPrefix, LPWSTR pszDirectoryPath )
{
    HRESULT hr = S_OK;
    LPWSTR pszSkippedDirectory = pszDirectoryPath;

    _ASSERT( pszDirectoryPath != NULL );

    if(pszSkipPrefix != NULL)
    {
        wcsstr( pszDirectoryPath, pszSkipPrefix );        
        pszSkippedDirectory = pszSkippedDirectory + wcslen( pszSkipPrefix );
    }

    LPWSTR  slash = wcschr( pszSkippedDirectory, L'\\' );
    DWORD   dwResult = 0;
    LPWSTR  previousSlash = NULL;

    while( slash != NULL )
    {
        pszSkippedDirectory = slash + 1; // move to next slash
        *slash = L'\0';
        if(!DirectoryExists(pszDirectoryPath))
        {
            if(!CreateDirectoryW( pszDirectoryPath, NULL ))
            {
                // directory creation failed, continue to create if ERROR_ALREADY_EXISTS
                if(dwResult != ERROR_ALREADY_EXISTS)
                {
                    CheckError(HRESULT_FROM_WIN32(GetLastError()));
                }
            }
        }
        // temporarily replace \ with - so wcschr can find the next slash.
        *slash = L'-';
        previousSlash = slash;
        slash = wcschr( pszSkippedDirectory, L'\\' );
        *previousSlash = L'\\';
    }

    if(!DirectoryExists(pszDirectoryPath))
    {
        if(!CreateDirectoryW( pszDirectoryPath, NULL ))
        {
            // directory creation failed, continue to create if ERROR_ALREADY_EXISTS
            if(dwResult != ERROR_ALREADY_EXISTS)
            {
                CheckError(HRESULT_FROM_WIN32(GetLastError()));
            }
        }
    }
    hr = S_OK;
Error:
    return hr;
}

BOOL CNodeApplicationManager::DirectoryExists(LPCWSTR directoryPath)
{
    DWORD dwFileAttributes;

    dwFileAttributes = GetFileAttributesW(directoryPath);

    return ((dwFileAttributes != INVALID_FILE_ATTRIBUTES) && 
        (dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY));
}

HRESULT CNodeApplicationManager::GetOrCreateNodeApplication(IHttpContext* context, NodeDebugCommand debugCommand, BOOL allowCreate, CNodeApplication** application)
{
    HRESULT hr;
    DWORD physicalPathLength;
    PCWSTR physicalPath;

    CheckNull(application);
    CheckNull(context);

    physicalPath = context->GetScriptTranslated(&physicalPathLength);

    *application = this->TryGetExistingNodeApplication(physicalPath, physicalPathLength, ND_NONE != debugCommand);

    if (NULL == *application && allowCreate)
    {
        // this code path executes under exclusive this->srwlock

        ErrorIf(INVALID_FILE_ATTRIBUTES == GetFileAttributesW(physicalPath), GetLastError());        

        if (ND_NONE != debugCommand)
        {
            // request for debugger, e.g. http://foo.com/bar/hello.js/debug/*

            CheckError(this->GetOrCreateDebuggedNodeApplicationCore(physicalPath, physicalPathLength, debugCommand, context, application));
        }
        else
        {
            // regular application request, e.g. http://foo.com/bar/hello.js

            CheckError(this->GetOrCreateNodeApplicationCore(physicalPath, physicalPathLength, context, application));
        }        
    }

    return S_OK;
Error:

    return hr;
}

CNodeApplication* CNodeApplicationManager::TryGetExistingNodeApplication(PCWSTR physicalPath, DWORD physicalPathLength, BOOL debuggerRequest)
{
    CNodeApplication* application = NULL;
    NodeApplicationEntry* current = this->applications;
    while (NULL != current)
    {
        if (debuggerRequest == FALSE && debuggerRequest == current->nodeApplication->IsDebugger()
            && 0 == wcsncmp(physicalPath, current->nodeApplication->GetScriptName(), physicalPathLength))
        {
            application = current->nodeApplication;
            break;
        }
        else if(debuggerRequest == TRUE && debuggerRequest == current->nodeApplication->IsDebugger()
            && 0 == wcsncmp(physicalPath, current->nodeApplication->GetPeerApplication()->GetScriptName(), physicalPathLength))
        {
            application = current->nodeApplication;
            break;
        }

        current = current->next;
    }

    return application;
}

HANDLE CNodeApplicationManager::GetJobObject()
{
    return this->jobObject;
}

BOOL CNodeApplicationManager::GetBreakAwayFromJobObject()
{
    return this->breakAwayFromJobObject;
}

CNodeEventProvider* CNodeApplicationManager::GetEventProvider()
{
    return this->eventProvider;
}

CFileWatcher* CNodeApplicationManager::GetFileWatcher()
{
    return this->fileWatcher;
}

HRESULT CNodeApplicationManager::FindNextDebugPort(IHttpContext* context, DWORD* port)
{
    HRESULT hr;
    DWORD start, end;

    CheckError(CModuleConfiguration::GetDebugPortRange(context, &start, &end));
    if (this->currentDebugPort < start || this->currentDebugPort > end)
    {
        this->currentDebugPort = start;
    }

    *port = this->currentDebugPort;
    BOOL found = FALSE;

    do {
        SOCKET listenSocket = INVALID_SOCKET;
        sockaddr_in service;
        if (INVALID_SOCKET != (listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)))
        {
            service.sin_family = AF_INET;
            service.sin_addr.s_addr = inet_addr("127.0.0.1");
            service.sin_port = htons(*port);
            if (SOCKET_ERROR != bind(listenSocket, (SOCKADDR*)&service, sizeof service))
            {
                if (SOCKET_ERROR != listen(listenSocket, 1))
                {
                    found = TRUE;
                }
            }

            closesocket(listenSocket);
        }

        if (!found)
        {
            (*port)++;
            if ((*port) > end)
                (*port) = start;
        }

    } while (!found && (*port) != this->currentDebugPort);

    if (found)
    {
        this->currentDebugPort = *port + 1;
        if (this->currentDebugPort > end)
            this->currentDebugPort = start;
    }
    else
    {
        *port = 0;
    }

    return found ? S_OK : IISNODE_ERROR_UNABLE_TO_FIND_DEBUGGING_PORT;
Error:
    return hr;
}

HRESULT CNodeApplicationManager::EnsureDebugeeReady(IHttpContext* context, DWORD debugPort)
{
#if TRUE // TODO, tjanczuk, figure out timing issues with debugee connectivity; connecting and disconnecting here
         // was causing later connect by the debugger to be unsuccessful for some reason
         // https://github.com/tjanczuk/iisnode/issues/71
    Sleep(1000);
    return S_OK;
#else
    DWORD retryCount = CModuleConfiguration::GetMaxNamedPipeConnectionRetry(context);
    DWORD delay = CModuleConfiguration::GetNamedPipeConnectionRetryDelay(context);

    DWORD retry = 0;
    do {
        SOCKET client = INVALID_SOCKET;
        sockaddr_in service;
        if (INVALID_SOCKET != (client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)))
        {
            service.sin_family = AF_INET;
            service.sin_addr.s_addr = inet_addr("127.0.0.1");
            service.sin_port = htons(debugPort);
            int result = connect(client, (SOCKADDR*)&service, sizeof service);
            closesocket(client);
            if (SOCKET_ERROR != result)
            {
                return S_OK;
            }
        }

        retry++;
        if (retry <= retryCount)
        {
            Sleep(delay);
        }

    } while (retry <= retryCount);

    return IISNODE_ERROR_UNABLE_TO_CONNECT_TO_DEBUGEE;
#endif
}