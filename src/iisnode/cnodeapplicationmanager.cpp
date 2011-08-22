#include "precomp.h"

CNodeApplicationManager::CNodeApplicationManager(IHttpServer* server, HTTP_MODULE_ID moduleId)
	: server(server), moduleId(moduleId), applications(NULL), asyncManager(NULL), jobObject(NULL), 
	breakAwayFromJobObject(FALSE), fileWatcher(NULL)
{
	InitializeCriticalSection(&this->syncRoot);
}

HRESULT CNodeApplicationManager::Initialize()
{
	HRESULT hr;
	BOOL isInJob, createJob;
	JOBOBJECT_EXTENDED_LIMIT_INFORMATION jobInfo;

	ErrorIf(NULL != this->asyncManager, ERROR_INVALID_OPERATION);
	ErrorIf(NULL == (this->asyncManager = new CAsyncManager()), ERROR_NOT_ENOUGH_MEMORY);
	CheckError(this->asyncManager->Initialize());
	ErrorIf(NULL == (this->fileWatcher = new CFileWatcher()), ERROR_NOT_ENOUGH_MEMORY);
	CheckError(this->fileWatcher->Initialize());

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

	return S_OK;
Error:

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

HRESULT CNodeApplicationManager::GetOrCreateNodeApplicationCore(PCWSTR physicalPath, CNodeApplication** application)
{
	HRESULT hr;	
	NodeApplicationEntry* applicationEntry = NULL;

	if (NULL == (*application = this->TryGetExistingNodeApplication(physicalPath)))
	{
		ErrorIf(NULL == (applicationEntry = new NodeApplicationEntry()), ERROR_NOT_ENOUGH_MEMORY);
		ErrorIf(NULL == (applicationEntry->nodeApplication = new CNodeApplication(this)), ERROR_NOT_ENOUGH_MEMORY);
		CheckError(applicationEntry->nodeApplication->Initialize(physicalPath, this->fileWatcher));

		*application = applicationEntry->nodeApplication;
		applicationEntry->next = this->applications;
		this->applications = applicationEntry;
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

		CheckError(this->GetOrCreateNodeApplicationCore(physicalPath, application));

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
