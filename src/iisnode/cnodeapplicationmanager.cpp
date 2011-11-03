#include "precomp.h"

CNodeApplicationManager::CNodeApplicationManager(IHttpServer* server, HTTP_MODULE_ID moduleId)
	: server(server), moduleId(moduleId), applications(NULL), asyncManager(NULL), jobObject(NULL), 
	breakAwayFromJobObject(FALSE), fileWatcher(NULL), initialized(FALSE), eventProvider(NULL),
	currentDebugPort(0)
{
	InitializeCriticalSection(&this->syncRoot);
}

HRESULT CNodeApplicationManager::Initialize(IHttpContext* context)
{
	HRESULT hr;
	BOOL isInJob, createJob;
	JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobInfo;

	if (this->initialized)
	{
		return S_OK;
	}

	ErrorIf(NULL == (this->eventProvider = new CNodeEventProvider()), ERROR_NOT_ENOUGH_MEMORY);
	CheckError(this->eventProvider->Initialize());
	ErrorIf(NULL != this->asyncManager, ERROR_INVALID_OPERATION);
	ErrorIf(NULL == (this->asyncManager = new CAsyncManager()), ERROR_NOT_ENOUGH_MEMORY);
	CheckError(this->asyncManager->Initialize(context));
	ErrorIf(NULL == (this->fileWatcher = new CFileWatcher()), ERROR_NOT_ENOUGH_MEMORY);
	CheckError(this->fileWatcher->Initialize(context));

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

	this->initialized = TRUE;

	this->GetEventProvider()->Log(L"iisnode initialized the application manager", WINEVENT_LEVEL_INFO);

	return S_OK;
Error:

	this->GetEventProvider()->Log(L"iisnode failed to initialize the application manager", WINEVENT_LEVEL_ERROR);

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
	while (NULL != this->applications)
	{
		delete this->applications->nodeApplication;
		NodeApplicationEntry* current = this->applications;
		this->applications = this->applications->next;
		delete current;
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

	DeleteCriticalSection(&this->syncRoot);
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
	HRESULT hr;
	CNodeApplication* application;
	NodeDebugCommand debugCommand;

	CheckNull(ctx);
	*ctx = NULL;

	CheckError(CNodeDebugger::GetDebugCommand(context, this->GetEventProvider(), &debugCommand));

	if (ND_KILL == debugCommand)
	{
		ENTER_CS(this->syncRoot)

		CheckError(this->EnsureDebuggedApplicationKilled(context, ctx));

		LEAVE_CS(this->syncRoot)
	}
	else if (ND_REDIRECT == debugCommand)
	{
		// redirection from e.g. app.js/debug to app.js/debug/

		CheckError(this->DebugRedirect(context, ctx));
	}
	else
	{
		CheckError(this->GetOrCreateNodeApplication(context, debugCommand, &application));
		CheckError(application->Enqueue(context, pProvider, ctx));
	}

	return S_OK;
Error:
	return hr;
}

HRESULT CNodeApplicationManager::DebugRedirect(IHttpContext* context, CNodeHttpStoredContext** ctx)
{
	HRESULT hr;

	ErrorIf(NULL == (*ctx = new CNodeHttpStoredContext(NULL, context)), ERROR_NOT_ENOUGH_MEMORY);
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
	CheckError(this->RecycleApplication(app));
	app = this->TryGetExistingNodeApplication(physicalPath, physicalPathLength, FALSE);
	if (app) killCount++;
	CheckError(this->RecycleApplication(app));

	if (ctx)
	{
		ErrorIf(NULL == (*ctx = new CNodeHttpStoredContext(NULL, context)), ERROR_NOT_ENOUGH_MEMORY);
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

HRESULT CNodeApplicationManager::RecycleApplication(CNodeApplication* app)
{
	if (app)
	{
		ENTER_CS(this->syncRoot)

		this->RecycleApplicationCore(app->GetPeerApplication());
		this->RecycleApplicationCore(app);

		LEAVE_CS(this->syncRoot)
	}

	return S_OK;
}

void CNodeApplicationManager::OnScriptModified(CNodeApplicationManager* manager, CNodeApplication* application)
{
	ENTER_CS(manager->syncRoot)
	
	// ensure the application still exists to avoid race condition with other recycling code paths

	NodeApplicationEntry* current = manager->applications;
	while (current)
	{
		if (current->nodeApplication == application)
		{
			manager->RecycleApplication(application);
			break;
		}

		current = current->next;
	}
	
	LEAVE_CS(manager->syncRoot)
}

// this must be called under lock
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
		this->GetEventProvider()->Log(L"iisnode found an existing node.js application to dispatch the http request to", WINEVENT_LEVEL_VERBOSE);
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
	
	this->GetEventProvider()->Log(L"iisnode failed to create a new node.js application", WINEVENT_LEVEL_ERROR);

	return hr;
}

HRESULT CNodeApplicationManager::EnsureDebuggerFilesInstalled(PWSTR physicalPath, DWORD physicalPathSize)
{
	HRESULT hr;
	HMODULE iisnode;
	DebuggerFileEnumeratorParams params = { S_OK, physicalPath, wcslen(physicalPath), physicalPathSize };

	// Process all resources of DEBUGGERFILE type (256) defined in resource.rc and save to disk if necessary
	
	ErrorIf(NULL == (iisnode = GetModuleHandle("iisnode.dll")), GetLastError());
	ErrorIf(!EnumResourceNames(iisnode, "DEBUGGERFILE", CNodeApplicationManager::EnsureDebuggerFile, (LONG_PTR)&params), GetLastError());
	CheckError(params.hr);

	this->GetEventProvider()->Log(
		L"iisnode unpacked debugger files", WINEVENT_LEVEL_INFO);

	return S_OK;
Error:

	this->GetEventProvider()->Log(
		L"iisnode failed to unpack debugger files", WINEVENT_LEVEL_ERROR);

	return hr;
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

	ErrorIf(NULL == lpszName, ERROR_INVALID_PARAMETER);

	// append file name to the base directory name

	CheckError(mbstowcs_s(
		&converted, 
		params->physicalFile + params->physicalFileLength, 
		params->physicalFileSize - params->physicalFileLength - 1, 
		lpszName, 
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

	// check if the file already exists and create it if it does not

	file = CreateFileW(params->physicalFile, GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
	
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

		DWORD debuggerPathSize = wcslen(physicalPath) + CModuleConfiguration::GetDebuggerPathSegmentLength(context) + 512;
		ErrorIf(NULL == (debuggerPath = (PWSTR)context->AllocateRequestMemory(sizeof WCHAR * debuggerPathSize)), ERROR_NOT_ENOUGH_MEMORY);
		swprintf(debuggerPath, L"%s.%s\\", physicalPath, CModuleConfiguration::GetDebuggerPathSegment(context));
		CheckError(this->EnsureDebuggerFilesInstalled(debuggerPath, debuggerPathSize));

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
		this->GetEventProvider()->Log(L"iisnode found an existing node.js debugger to dispatch the http request to", WINEVENT_LEVEL_VERBOSE);
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

	this->GetEventProvider()->Log(L"iisnode failed to create a new node.js application to debug or the debugger for that application", WINEVENT_LEVEL_ERROR);

	return hr;
}

HRESULT CNodeApplicationManager::GetOrCreateNodeApplication(IHttpContext* context, NodeDebugCommand debugCommand, CNodeApplication** application)
{
	HRESULT hr;
	DWORD physicalPathLength;
	PCWSTR physicalPath;

	CheckNull(application);
	CheckNull(context);

	physicalPath = context->GetScriptTranslated(&physicalPathLength);

	ENTER_CS(this->syncRoot)

	*application = this->TryGetExistingNodeApplication(physicalPath, physicalPathLength, ND_NONE != debugCommand);

	if (NULL == *application)
	{
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

	LEAVE_CS(this->syncRoot)

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
		if (debuggerRequest == current->nodeApplication->IsDebugger()
			&& 0 == wcsncmp(physicalPath, current->nodeApplication->GetScriptName(), physicalPathLength))
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
}
