#include "precomp.h"

CNodeApplication::CNodeApplication(CNodeApplicationManager* applicationManager)
	: applicationManager(applicationManager), scriptName(NULL), pendingRequests(NULL)
{
}

CNodeApplication::~CNodeApplication()
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
}

HRESULT CNodeApplication::Initialize(PCWSTR scriptName)
{
	HRESULT hr;

	ErrorIf(NULL == scriptName, ERROR_INVALID_PARAMETER);

	DWORD len = wcslen(scriptName);
	ErrorIf(NULL == (this->scriptName = new WCHAR[len + 1]), ERROR_NOT_ENOUGH_MEMORY);
	memcpy(this->scriptName, scriptName, sizeof(WCHAR) * len);
	this->scriptName[sizeof(WCHAR) * len] = L'\0';

	ErrorIf(NULL == (this->pendingRequests = new CPendingRequestQueue()), ERROR_NOT_ENOUGH_MEMORY);

	return S_OK;
Error:
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

HRESULT CNodeApplication::StartNewRequest(IHttpContext* context, IHttpEventProvider* pProvider)
{
	HRESULT hr;
	CNodeHttpStoredContext* nodeContext;

	ErrorIf(NULL == context, ERROR_INVALID_PARAMETER);
	ErrorIf(NULL == pProvider, ERROR_INVALID_PARAMETER);

	ErrorIf(NULL == (nodeContext = new CNodeHttpStoredContext(this, context)), ERROR_NOT_ENOUGH_MEMORY);

	IHttpModuleContextContainer* moduleContextContainer = context->GetModuleContextContainer();
	moduleContextContainer->SetModuleContext(nodeContext, this->GetApplicationManager()->GetModuleId());

	return S_OK;
Error:
	return hr;
}