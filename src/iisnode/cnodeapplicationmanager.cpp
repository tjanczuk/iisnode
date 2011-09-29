#include "precomp.h"

CNodeApplicationManager::CNodeApplicationManager(IHttpServer* server, HTTP_MODULE_ID moduleId)
	: server(server), moduleId(moduleId), applications(NULL), asyncManager(NULL), jobObject(NULL), 
	breakAwayFromJobObject(FALSE), fileWatcher(NULL), initialized(FALSE), eventProvider(NULL)
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

	this->GetEventProvider()->Log(L"iisnode has initialized the application manager", WINEVENT_LEVEL_INFO);

	return S_OK;
Error:

	this->GetEventProvider()->Log(L"iisnode has failed to initialize the application manager", WINEVENT_LEVEL_ERROR);

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

HRESULT CNodeApplicationManager::Dispatch(IHttpContext* context, IHttpEventProvider* pProvider)
{
	HRESULT hr;
	CNodeApplication* application;

	CheckError(this->GetOrCreateNodeApplication(context, &application));
	CheckError(application->Enqueue(context, pProvider));

	return S_OK;
Error:
	return hr;
}

HRESULT CNodeApplicationManager::GetOrCreateNodeApplicationCore(PCWSTR physicalPath, IHttpContext* context, CNodeApplication** application)
{
	HRESULT hr;	
	NodeApplicationEntry* applicationEntry = NULL;

	if (NULL == (*application = this->TryGetExistingNodeApplication(physicalPath)))
	{
		ErrorIf(NULL == (applicationEntry = new NodeApplicationEntry()), ERROR_NOT_ENOUGH_MEMORY);
		ErrorIf(NULL == (applicationEntry->nodeApplication = new CNodeApplication(this)), ERROR_NOT_ENOUGH_MEMORY);
		CheckError(applicationEntry->nodeApplication->Initialize(physicalPath, context, this->fileWatcher));

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

HRESULT CNodeApplicationManager::GetOrCreateNodeApplication(IHttpContext* context, CNodeApplication** application)
{
	HRESULT hr;
	NodeApplicationEntry* newApplicationEntry = NULL;

	ErrorIf(NULL == application, ERROR_INVALID_PARAMETER);
	ErrorIf(NULL == context, ERROR_INVALID_PARAMETER);

	DWORD physicalPathLength;
	PCWSTR physicalPath = context->GetPhysicalPath(&physicalPathLength);
	ErrorIf(0 == physicalPathLength, ERROR_INVALID_PARAMETER);

	if (NULL == (*application = this->TryGetExistingNodeApplication(physicalPath)))
	{
		ErrorIf(INVALID_FILE_ATTRIBUTES == GetFileAttributesW(physicalPath), GetLastError());

		ENTER_CS(this->syncRoot)

		CheckError(this->GetOrCreateNodeApplicationCore(physicalPath, context, application));

		LEAVE_CS(this->syncRoot)
	}

	return S_OK;
Error:

	return hr;
}

CNodeApplication* CNodeApplicationManager::TryGetExistingNodeApplication(PCWSTR physicalPath)
{
	CNodeApplication* application = NULL;
	NodeApplicationEntry* current = this->applications;
	while (NULL != current)
	{
		if (0 == wcscmp(physicalPath, current->nodeApplication->GetScriptName()))
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
