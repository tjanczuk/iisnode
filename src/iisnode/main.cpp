#include "precomp.h"



HRESULT __stdcall RegisterModule(
    DWORD                           dwServerVersion,
    IHttpModuleRegistrationInfo *   pModuleInfo,
    IHttpServer *                   pHttpServer
)
{
    HRESULT hr;
    CNodeHttpModuleFactory* pFactory = NULL;

	ErrorIf(pModuleInfo == NULL || pHttpServer == NULL, HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER));

    ErrorIf(NULL == (pFactory = new CNodeHttpModuleFactory()), HRESULT_FROM_WIN32(ERROR_NOT_ENOUGH_MEMORY));
	CheckError(pFactory->Initialize(pHttpServer, pModuleInfo->GetId()));

    CheckError(pModuleInfo->SetRequestNotifications(pFactory, RQ_EXECUTE_REQUEST_HANDLER, 0));
    
	return S_OK;

Error:
    
    if ( pFactory != NULL )
    {
        delete pFactory;
    }   

    return hr;
}
