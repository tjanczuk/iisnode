#include "precomp.h"

CNodeApplication::CNodeApplication(CNodeApplicationManager* applicationManager, BOOL isDebugger, NodeDebugCommand debugCommand, DWORD debugPort)
	: applicationManager(applicationManager), scriptName(NULL), pendingRequests(NULL), processManager(NULL),
	isDebugger(isDebugger), peerApplication(NULL), debugCommand(debugCommand), debugPort(debugPort)
{
}

CNodeApplication::~CNodeApplication()
{
	this->Cleanup();
}

void CNodeApplication::Cleanup()
{
	this->GetApplicationManager()->GetFileWatcher()->RemoveWatch(this);

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
		this->processManager->DecRef(); // incremented in CNodeProcessManager::ctor
		this->processManager = NULL;
	}
}

HRESULT CNodeApplication::Initialize(PCWSTR scriptName, IHttpContext* context)
{
	HRESULT hr;

	CheckNull(scriptName);

	DWORD len = wcslen(scriptName) + 1;
	ErrorIf(NULL == (this->scriptName = new WCHAR[len]), ERROR_NOT_ENOUGH_MEMORY);
	wcscpy(this->scriptName, scriptName);

	ErrorIf(NULL == (this->processManager = new	CNodeProcessManager(this, context)), ERROR_NOT_ENOUGH_MEMORY);
	CheckError(this->processManager->Initialize(context));

	ErrorIf(NULL == (this->pendingRequests = new CPendingRequestQueue()), ERROR_NOT_ENOUGH_MEMORY);
	CheckError(this->pendingRequests->Initialize());

	CheckError(this->GetApplicationManager()->GetFileWatcher()->WatchFile(
		scriptName, 
		CNodeApplicationManager::OnScriptModified, 
		this->GetApplicationManager(), 
		this));

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

HRESULT CNodeApplication::Enqueue(IHttpContext* context, IHttpEventProvider* pProvider, CNodeHttpStoredContext** ctx)
{
	HRESULT hr;

	CheckNull(ctx);
	CheckNull(context);
	CheckNull(pProvider);

	ErrorIf(NULL == (*ctx = new CNodeHttpStoredContext(this, context)), ERROR_NOT_ENOUGH_MEMORY);	
	IHttpModuleContextContainer* moduleContextContainer = context->GetModuleContextContainer();
	moduleContextContainer->SetModuleContext(*ctx, this->GetApplicationManager()->GetModuleId());

	if (S_OK == (hr = this->pendingRequests->Push(*ctx)))
	{		
		this->processManager->PostDispatchOneRequest();		
	}
	else
	{
		return hr;
	}

	return S_OK;
Error:

	// nodeContext need not be freed here as it will be deallocated through IHttpStoredContext when the request is finished

	return hr;
}

CNodeApplication* CNodeApplication::GetPeerApplication()
{
	return this->peerApplication;
}

void CNodeApplication::SetPeerApplication(CNodeApplication* peerApplication)
{
	this->peerApplication = peerApplication;
}

BOOL CNodeApplication::IsDebugger()
{
	return this->isDebugger;
}

BOOL CNodeApplication::IsDebuggee()
{
	return !this->IsDebugger() && NULL != this->GetPeerApplication();
}

BOOL CNodeApplication::IsDebugMode()
{
	return this->IsDebuggee() || this->IsDebugger();
}

NodeDebugCommand CNodeApplication::GetDebugCommand()
{
	return this->debugCommand;
}

HRESULT CNodeApplication::Recycle()
{
	return this->processManager->Recycle();
}

DWORD CNodeApplication::GetDebugPort()
{
	return this->debugPort;
}
