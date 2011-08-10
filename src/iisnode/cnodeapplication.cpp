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

HRESULT CNodeApplication::Initialize(PCWSTR scriptName)
{
	HRESULT hr;

	ErrorIf(NULL == scriptName, ERROR_INVALID_PARAMETER);

	DWORD len = wcslen(scriptName);
	ErrorIf(NULL == (this->scriptName = new WCHAR[len + 1]), ERROR_NOT_ENOUGH_MEMORY);
	memcpy(this->scriptName, scriptName, sizeof(WCHAR) * len);
	this->scriptName[sizeof(WCHAR) * len] = L'\0';

	ErrorIf(NULL == (this->processManager = new	CNodeProcessManager(this)), ERROR_NOT_ENOUGH_MEMORY);
	CheckError(this->processManager->Initialize());

	ErrorIf(NULL == (this->pendingRequests = new CPendingRequestQueue()), ERROR_NOT_ENOUGH_MEMORY);

	return S_OK;
Error:

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

HRESULT CNodeApplication::StartNewRequest(IHttpContext* context, IHttpEventProvider* pProvider)
{
	HRESULT hr;
	CNodeHttpStoredContext* nodeContext = NULL;

	ErrorIf(NULL == context, ERROR_INVALID_PARAMETER);
	ErrorIf(NULL == pProvider, ERROR_INVALID_PARAMETER);

	ErrorIf(NULL == (nodeContext = new CNodeHttpStoredContext(this, context)), ERROR_NOT_ENOUGH_MEMORY);
	// TODO, tjanczuk, return 503 if the pending requests quota has been reached
	CheckError(this->pendingRequests->Push(nodeContext));

	IHttpModuleContextContainer* moduleContextContainer = context->GetModuleContextContainer();
	moduleContextContainer->SetModuleContext(nodeContext, this->GetApplicationManager()->GetModuleId());
	nodeContext = NULL;

	CheckError(this->processManager->TryDispatchOneRequest());

	return S_OK;
Error:

	if (NULL != nodeContext)
	{
		delete nodeContext;
		nodeContext = NULL;
	}

	return hr;
}