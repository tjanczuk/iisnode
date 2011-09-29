#include "precomp.h"

CNodeApplication::CNodeApplication(CNodeApplicationManager* applicationManager)
	: applicationManager(applicationManager), scriptName(NULL), pendingRequests(NULL), processManager(NULL)
{
}

CNodeApplication::~CNodeApplication()
{
	this->Cleanup();
}

void CNodeApplication::Cleanup()
{
	if (NULL != this->scriptName)
	{
		delete [] this->scriptName;
		this->scriptName = NULL;
	}

	if (NULL != this->pendingRequests)
	{
		delete this->pendingRequests;
		this->pendingRequests = NULL;
	}

	if (NULL != this->processManager)
	{
		delete this->processManager;
		this->processManager = NULL;
	}
}

HRESULT CNodeApplication::Initialize(PCWSTR scriptName, IHttpContext* context, CFileWatcher* fileWatcher)
{
	HRESULT hr;

	CheckNull(scriptName);
	CheckNull(fileWatcher);

	DWORD len = wcslen(scriptName) + 1;
	ErrorIf(NULL == (this->scriptName = new WCHAR[len]), ERROR_NOT_ENOUGH_MEMORY);
	memcpy(this->scriptName, scriptName, sizeof(WCHAR) * len);

	ErrorIf(NULL == (this->processManager = new	CNodeProcessManager(this, context)), ERROR_NOT_ENOUGH_MEMORY);
	CheckError(this->processManager->Initialize());

	ErrorIf(NULL == (this->pendingRequests = new CPendingRequestQueue()), ERROR_NOT_ENOUGH_MEMORY);

	CheckError(fileWatcher->WatchFile(scriptName, CNodeApplication::OnScriptModified, this));

	this->GetApplicationManager()->GetEventProvider()->Log(L"iisnode initialized a new node.js application", WINEVENT_LEVEL_INFO);

	return S_OK;
Error:

	this->GetApplicationManager()->GetEventProvider()->Log(L"iisnode failed to initialize a new node.js application", WINEVENT_LEVEL_ERROR);

	this->Cleanup();

	return hr;
}

PCWSTR CNodeApplication::GetScriptName()
{
	return this->scriptName;
}

CNodeApplicationManager* CNodeApplication::GetApplicationManager()
{
	return this->applicationManager;
}

CPendingRequestQueue* CNodeApplication::GetPendingRequestQueue()
{
	return this->pendingRequests;
}

HRESULT CNodeApplication::Enqueue(IHttpContext* context, IHttpEventProvider* pProvider)
{
	HRESULT hr;
	CNodeHttpStoredContext* nodeContext;

	CheckNull(context);
	CheckNull(pProvider);

	ErrorIf(NULL == (nodeContext = new CNodeHttpStoredContext(this, context)), ERROR_NOT_ENOUGH_MEMORY);	
	IHttpModuleContextContainer* moduleContextContainer = context->GetModuleContextContainer();
	moduleContextContainer->SetModuleContext(nodeContext, this->GetApplicationManager()->GetModuleId());

	CheckError(this->pendingRequests->Push(nodeContext));
	
	this->GetApplicationManager()->GetAsyncManager()->PostContinuation(CNodeProcessManager::TryDispatchOneRequest, this->processManager);

	return S_OK;
Error:

	// nodeContext need not be freed here as it will be deallocated through IHttpStoredContext when the request is finished

	return hr;
}

void CNodeApplication::OnScriptModified(PCWSTR fileName, void* data)
{
	((CNodeApplication*)data)->processManager->RecycleAllProcesses();
}