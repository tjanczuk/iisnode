#ifndef __UTILS_H__
#define __UTILS_H__

#define ErrorIf(expr, hresult) if (expr) { hr = hresult; goto Error; }

#define CheckNull(expr) ErrorIf(NULL == expr, ERROR_INVALID_PARAMETER)

#define CheckError(hresult) { HRESULT tmp_hr = hresult; if (S_OK != tmp_hr) { hr = tmp_hr; goto Error; } }

#define ENTER_CS(cs) EnterCriticalSection(&cs); __try {

#define LEAVE_CS(cs) } __finally { LeaveCriticalSection(&cs); }

#define ENTER_SRW_SHARED(srw) AcquireSRWLockShared(&srw); __try {

#define LEAVE_SRW_SHARED(srw) } __finally { ReleaseSRWLockShared(&srw); }

#define ENTER_SRW_EXCLUSIVE(srw) AcquireSRWLockExclusive(&srw); __try {

#define LEAVE_SRW_EXCLUSIVE(srw) } __finally { ReleaseSRWLockExclusive(&srw); }

struct ENUM_INDEX
{
    VARIANT Index;
    ULONG Count;
};

class CUtils
{
public:
    ~CUtils()
    {
    }

    static 
    HRESULT 
    GetElementDWORDProperty(
        IAppHostElement     *pElement,
        LPCWSTR              pszPropName,
        DWORD               *pdwPropValue
    )
    {
        HRESULT             hr = S_OK;
        BSTR                bstrPropName = SysAllocString( pszPropName );
        IAppHostProperty   *pProperty = NULL;
        VARIANT             varValue;

        VariantInit(&varValue);

        ErrorIf(bstrPropName == NULL, E_OUTOFMEMORY);
        CheckError(pElement->GetPropertyByName( bstrPropName, &pProperty ));
        CheckError(pProperty->get_Value( &varValue ));
        CheckError(VariantChangeType( &varValue, &varValue, 0, VT_UI4 ));

        *pdwPropValue = varValue.ulVal;

        hr = S_OK;
    Error:
        VariantClear( &varValue );

        if(pProperty)
        {
            pProperty->Release();
            pProperty = NULL;
        }

        if(bstrPropName)
        {
            SysFreeString(bstrPropName);
        }

        return hr;
    }

    static
    HRESULT
    GetAdminElement(
        IN      IAppHostAdminManager *      pAdminMgr,
        IN      LPCWSTR                     pszConfigPath,
        IN      LPCWSTR                     pszElementName,
        OUT     IAppHostElement **          ppElement
    )
    {
        HRESULT hr = S_OK;
        BSTR bstrConfigPath = NULL;
        BSTR bstrElementName = NULL;

        bstrConfigPath = SysAllocString(pszConfigPath);
        bstrElementName = SysAllocString(pszElementName);

        ErrorIf(bstrConfigPath == NULL, E_OUTOFMEMORY);
        ErrorIf(bstrElementName == NULL, E_OUTOFMEMORY);

        CheckError(pAdminMgr->GetAdminSection( bstrElementName,
                                         bstrConfigPath,
                                         ppElement ));

        hr = S_OK; //do cleanup
    Error:

        if ( bstrElementName != NULL )
        {
            SysFreeString(bstrElementName);
            bstrElementName = NULL;
        }
        if ( bstrConfigPath != NULL )
        {
            SysFreeString(bstrConfigPath);
            bstrConfigPath = NULL;
        }

        return hr;
    }

    static
    HRESULT
    GetElementStringProperty(
        IN          IAppHostElement    *pElement,
        IN          LPCWSTR             pszPropName,
        OUT         BSTR               *ppbstrPropValue
    )
    {
        HRESULT hr = S_OK;
        BSTR bstrPropName = SysAllocString( pszPropName );
        IAppHostProperty* pProperty = NULL;

        *ppbstrPropValue = NULL;

        ErrorIf(bstrPropName == NULL, E_OUTOFMEMORY);

        CheckError(pElement->GetPropertyByName( bstrPropName, &pProperty ));

        CheckError(pProperty->get_StringValue( ppbstrPropValue ));

        hr = S_OK; // do cleanup
    Error:
        if (pProperty)
        {
            pProperty->Release();
        }

        if (bstrPropName)
        {
            SysFreeString( bstrPropName );
        }

        return hr;
    }

    static
    HRESULT
    FindNextElement(
        IN      IAppHostElementCollection          *pCollection,
        IN OUT  ENUM_INDEX                         *pIndex,
        OUT     IAppHostElement                   **ppElement
    )
    {
        HRESULT hr;

        *ppElement = NULL;

        if (pIndex->Index.ulVal >= pIndex->Count)
        {
            return S_FALSE;
        }

        hr = pCollection->get_Item(pIndex->Index, ppElement);

        if (SUCCEEDED(hr))
        {
            pIndex->Index.ulVal++;
        }

        return hr;
    }

    static
    HRESULT
    FindFirstElement(
        IN      IAppHostElementCollection          *pCollection,
        OUT     ENUM_INDEX                         *pIndex,
        OUT     IAppHostElement                   **ppElement
    )
    {
        HRESULT hr;

        CheckError(pCollection->get_Count(&pIndex->Count));

        //TODO: check if VariantClear is needed in this case.
        VariantInit(&pIndex->Index);
        pIndex->Index.vt = VT_UI4;
        pIndex->Index.ulVal = 0;

        return FindNextElement(pCollection, pIndex, ppElement);
    Error:
        return hr;
    }

private:
    CUtils()
    {
    }
};

#endif