#include "precomp.h"

CNodeApplicationManager::CNodeApplicationManager(IHttpServer* server, HTTP_MODULE_ID moduleId)
	: server(server), moduleId(moduleId), applications(NULL), asyncManager(NULL)
{
	InitializeCriticalSection(&this->syncRoot);
}

HRESULT CNodeApplicationManager::Initialize()
{
	HRESULT hr;

	ErrorIf(NULL != this->asyncManager, ERROR_INVALID_OPERATION);
	ErrorIf(NULL == (this->asyncManager = new CAsyncManager()), ERROR_NOT_ENOUGH_MEMORY);
	CheckError(this->asyncManager->Initialize());

	return S_OK;
Error:
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
		CheckError(applicationEntry->nodeApplication->Initialize(physicalPath));

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