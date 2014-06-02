#include "precomp.h"

RtlNtStatusToDosError pRtlNtStatusToDosError;


HRESULT __stdcall RegisterModule(
    DWORD                           dwServerVersion,
    IHttpModuleRegistrationInfo *   pModuleInfo,
    IHttpServer *                   pHttpServer
)
{
    HRESULT hr;
    HMODULE hNtDll;
    CNodeHttpModuleFactory* pFactory = NULL;
    CNodeGlobalModule* pGlobalModule = NULL;

    ErrorIf(pModuleInfo == NULL || pHttpServer == NULL, HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER));

    hNtDll = GetModuleHandleA("ntdll.dll");
    ErrorIf(NULL == hNtDll, HRESULT_FROM_WIN32(GetLastError()));

    pRtlNtStatusToDosError = (RtlNtStatusToDosError) GetProcAddress(hNtDll, "RtlNtStatusToDosError");
    ErrorIf(NULL == pRtlNtStatusToDosError, HRESULT_FROM_WIN32(GetLastError()));

    ErrorIf(NULL == (pFactory = new CNodeHttpModuleFactory()), HRESULT_FROM_WIN32(ERROR_NOT_ENOUGH_MEMORY));
    CheckError(pFactory->Initialize(pHttpServer, pModuleInfo->GetId()));

    CheckError(pModuleInfo->SetRequestNotifications(pFactory, RQ_EXECUTE_REQUEST_HANDLER | RQ_SEND_RESPONSE, 0));
    
    ErrorIf(NULL == (pGlobalModule = new CNodeGlobalModule(pFactory->GetNodeApplicationManager())), E_OUTOFMEMORY);

    CheckError(pModuleInfo->SetGlobalNotifications( pGlobalModule, GL_CONFIGURATION_CHANGE ));

    return S_OK;

Error:
    
    if ( pFactory != NULL )
    {
        delete pFactory;
    }

    if( pGlobalModule != NULL )
    {
        delete pGlobalModule;
    }

    return hr;
}
