#ifndef __CNODEHTTPMODULEFACTORY_H__
#define __CNODEHTTPMODULEFACTORY_H__

class CNodeHttpModuleFactory : public IHttpModuleFactory
{
public:
    virtual
    HRESULT
    GetHttpModule(
        OUT CHttpModule            **ppModule, 
        IN IModuleAllocator        *
    )
    {
        HRESULT                    hr = S_OK;
        CNodeHttpModule *          pModule = NULL;

	    if ( ppModule == NULL )
        {
            hr = HRESULT_FROM_WIN32( ERROR_INVALID_PARAMETER );
            goto Finished;
        }

        pModule = new CNodeHttpModule();
        if ( pModule == NULL )
        {
            hr = HRESULT_FROM_WIN32( ERROR_NOT_ENOUGH_MEMORY );
            goto Finished;
        }

        *ppModule = pModule;
        pModule = NULL;
            
    Finished:

        if ( pModule != NULL )
        {
            delete pModule;
            pModule = NULL;
        }

        return hr;
    }

    virtual 
    void
    Terminate()
    {
        delete this;
    }
};

#endif
