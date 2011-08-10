#include "precomp.h"

// TODO, tjanczuk, hook up the implementation of the actual configuration system of the module

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

DWORD CModuleConfiguration::GetAsyncCompletionThreadCount()
{
	return 2;
}

DWORD CModuleConfiguration::GetMaxProcessCountPerApplication()
{
	return 2;
}

LPCTSTR CModuleConfiguration::GetNodeProcessCommandLine()
{
	return _T("node.exe");
}

DWORD CModuleConfiguration::GetMaxConcurrentRequestsPerProcess()
{
	return 1024;
}

DWORD CModuleConfiguration::GetMaxNamedPipeConnectionRetry()
{
	return 3;
}

DWORD CModuleConfiguration::GetNamePipeConnectionRetryDelay()
{
	return 5 * 10000; // this is 5ms expressed in 100 nanosecond units
}

DWORD CModuleConfiguration::GetInitialRequestBufferSize()
{
	return 4 * 1024;
}
