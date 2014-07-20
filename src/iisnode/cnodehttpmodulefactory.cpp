#include "precomp.h"

CNodeHttpModuleFactory::CNodeHttpModuleFactory() 
    : applicationManager(NULL)
{
}

CNodeHttpModuleFactory::~CNodeHttpModuleFactory()
{
    if (NULL != this->applicationManager)
    {
        delete this->applicationManager;
        this->applicationManager = NULL;
    }

    WSACleanup();
}

HRESULT CNodeHttpModuleFactory::Initialize(IHttpServer* server, HTTP_MODULE_ID moduleId) 
{
    HRESULT hr;
    WSADATA wsaData;

    ErrorIf(NULL == server, ERROR_INVALID_PARAMETER);
    ErrorIf(NULL == moduleId, ERROR_INVALID_PARAMETER);

    CModuleConfiguration::Initialize(server, moduleId);

    CheckError(WSAStartup(MAKEWORD(2,2), &wsaData));
    ErrorIf(NULL == (this->applicationManager = new CNodeApplicationManager(server, moduleId)), ERROR_NOT_ENOUGH_MEMORY);	

    return S_OK;
Error:
    return hr;
}

CNodeApplicationManager* CNodeHttpModuleFactory::GetNodeApplicationManager()
{
    return this->applicationManager;
}

HRESULT CNodeHttpModuleFactory::GetHttpModule(
    OUT CHttpModule **ppModule, 
    IN IModuleAllocator *
)
{
    HRESULT hr;

    ErrorIf(NULL == (*ppModule = new CNodeHttpModule(this->applicationManager)), ERROR_NOT_ENOUGH_MEMORY);

    return S_OK;
Error:
    return hr;
}

void CNodeHttpModuleFactory::Terminate()
{
    delete this;
}