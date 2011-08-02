#include "precomp.h"

//  Global server instance
IHttpServer *                       g_pHttpServer = NULL;

//  Global module context id
PVOID                               g_pModuleContext = NULL;

//  The RegisterModule entrypoint implementation.
//  This method is called by the server when the module DLL is 
//  loaded in order to create the module factory,
//  and register for server events.
HRESULT
__stdcall
RegisterModule(
    DWORD                           dwServerVersion,
    IHttpModuleRegistrationInfo *   pModuleInfo,
    IHttpServer *                   pHttpServer
)
{
    HRESULT                             hr = S_OK;
    CNodeHttpModuleFactory  *           pFactory = NULL;

    if ( pModuleInfo == NULL || pHttpServer == NULL )
    {
        hr = HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );
        goto Finished;
    }

    // step 1: save the IHttpServer and the module context id for future use
    g_pModuleContext = pModuleInfo->GetId();
    g_pHttpServer = pHttpServer;

    // step 2: create the module factory
    pFactory = new CNodeHttpModuleFactory();
    if ( pFactory == NULL )
    {
        hr = HRESULT_FROM_WIN32( ERROR_NOT_ENOUGH_MEMORY );
        goto Finished;
    }

    // step 3: register for server events
    // TODO: register for more server events here
    hr = pModuleInfo->SetRequestNotifications( pFactory, /* module factory */
                                               RQ_ACQUIRE_REQUEST_STATE /* server event mask */,
                                               0 /* server post event mask */);
    if ( FAILED( hr ) )
    {
        goto Finished;
    }

    pFactory = NULL;

Finished:
    
    if ( pFactory != NULL )
    {
        delete pFactory;
        pFactory = NULL;
    }   

    return hr;
}
