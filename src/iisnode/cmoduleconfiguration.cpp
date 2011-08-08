#include "precomp.h"

// TODO, tjanczuk, hook up the implementation the actual configuration system of the module

IHttpServer* CModuleConfiguration::server = NULL;

CModuleConfiguration::CModuleConfiguration()
{
}

CModuleConfiguration::~CModuleConfiguration()
{
}

HRESULT CModuleConfiguration::Initialize(IHttpServer* server)	
{
	HRESULT hr;

	ErrorIf(NULL == server, ERROR_INVALID_PARAMETER);

	CModuleConfiguration::server = server;

	return S_OK;
Error:
	return hr;
}

DWORD CModuleConfiguration::GetMaxPendingRequestsPerApplication()
{
	return 1024;
}

